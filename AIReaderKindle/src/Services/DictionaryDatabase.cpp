#include "DictionaryDatabase.hpp"

#include "../Domain/Dictionary/WordNormalizer.hpp"
#include "../Support/Files.hpp"
#include "../Support/Inflate.hpp"
#include "Support/Json.hpp"
#include "Paths.hpp"

#include <set>

DictionaryDatabase& DictionaryDatabase::shared() {
    static DictionaryDatabase database;
    return database;
}

std::string DictionaryDatabase::path(const DictionaryPack& pack) {
    if (pack.isBundled()) return Paths::bundledDictionary();
    return Files::join(Paths::dictionaries(), pack.fileName);
}

Database* DictionaryDatabase::connection(const std::string& path) {
    auto found = connections_.find(path);
    if (found != connections_.end()) return found->second.get();
    if (!Files::exists(path)) return nullptr;
    auto database = std::make_unique<Database>(path, true);
    if (!database->isOpen()) return nullptr;
    Database* raw = database.get();
    connections_[path] = std::move(database);
    return raw;
}

DictionaryLookup DictionaryDatabase::lookup(const std::string& word, const std::vector<DictionaryPack>& packs) {
    std::string normalized = WordNormalizer::normalize(word);
    DictionaryLookup result;
    result.query = normalized;
    if (normalized.empty()) return result;

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pack : packs) {
        Database* database = connection(path(pack));
        if (database) search(*database, normalized, pack.name, result);
    }
    return result;
}

std::vector<DictionaryLookup::Article> DictionaryDatabase::articlesFor(const std::string& lemma, const std::vector<DictionaryPack>& packs) {
    std::string wanted = WordNormalizer::normalize(lemma);
    std::vector<DictionaryLookup::Article> articles;
    for (auto& article : lookup(lemma, packs).articles) {
        if (WordNormalizer::normalize(article.lemma) == wanted) articles.push_back(std::move(article));
    }
    return articles;
}

namespace {

struct Lemma {
    long long id;
    std::string word;
};

std::vector<Lemma> lemmasNamed(Database& database, const std::string& word) {
    std::vector<Lemma> lemmas;
    Statement query(database, "SELECT id, word FROM lemmas WHERE word = ?");
    query.bind(1, word);
    while (query.step()) lemmas.push_back({query.integer(0), query.text(1)});
    return lemmas;
}

std::vector<Lemma> lemmasById(Database& database, const std::set<long long>& ids) {
    std::vector<Lemma> lemmas;
    Statement query(database, "SELECT id, word FROM lemmas WHERE id = ?");
    for (long long id : ids) {
        query.reset();
        query.bind(1, id);
        if (query.step()) lemmas.push_back({query.integer(0), query.text(1)});
    }
    return lemmas;
}

std::vector<std::string> features(const std::string& verbInfo) {
    std::vector<std::string> result;
    auto json = Json::parse(verbInfo);
    if (!json) return result;
    for (const auto& item : json->items()) {
        if (item.isString()) result.push_back(item.string());
    }
    return result;
}

/// One article as stored in an entry payload: zlib-compressed JSON with
/// `part_of_speech` and `definitions[].glosses[]`.
std::optional<DictionaryLookup::Article> article(const std::string& payload, const std::string& lemma, const std::string& source) {
    auto inflated = Inflate::zlib(payload);
    auto json = Json::parse(inflated ? *inflated : payload);
    if (!json) return std::nullopt;
    DictionaryLookup::Article result;
    result.lemma = lemma;
    result.partOfSpeech = json->at("part_of_speech").string();
    result.source = source;
    for (const auto& definition : json->at("definitions").items()) {
        for (const auto& gloss : definition.at("glosses").items()) {
            if (gloss.isString()) result.senses.push_back(gloss.string());
        }
    }
    if (result.senses.empty()) return std::nullopt;
    return result;
}

}  // namespace

void DictionaryDatabase::search(Database& database, const std::string& normalized, const std::string& source, DictionaryLookup& result) {
    struct FormRow {
        long long lemmaId;
        std::string partOfSpeech, gender, number, verbInfo;
    };
    std::vector<FormRow> forms;
    Statement formQuery(database,
        "SELECT lemma_id, part_of_speech, gender, number, verb_info FROM forms"
        " WHERE normalized_form = ? ORDER BY ordinal");
    formQuery.bind(1, normalized);
    while (formQuery.step()) {
        forms.push_back({formQuery.integer(0), formQuery.text(1), formQuery.text(2), formQuery.text(3), formQuery.text(4)});
    }

    std::set<long long> ids;
    for (const auto& form : forms) ids.insert(form.lemmaId);
    std::vector<Lemma> lemmas = forms.empty() ? lemmasNamed(database, normalized) : lemmasById(database, ids);
    // A form table hit whose lemma has no article is still worth reporting;
    // the direct headword may carry the definitions.
    if (lemmas.empty()) lemmas = lemmasNamed(database, normalized);

    std::map<long long, std::string> names;
    for (const auto& lemma : lemmas) names[lemma.id] = lemma.word;
    for (const auto& form : forms) {
        auto name = names.find(form.lemmaId);
        if (name == names.end()) continue;
        result.forms.push_back({name->second, form.partOfSpeech, form.gender, form.number, features(form.verbInfo)});
    }

    Statement entries(database, "SELECT payload FROM entries WHERE lemma_id = ? ORDER BY ordinal");
    for (const auto& lemma : lemmas) {
        entries.reset();
        entries.bind(1, lemma.id);
        while (entries.step()) {
            if (auto found = article(entries.blob(0), lemma.word, source)) result.articles.push_back(*found);
        }
    }
}

std::map<std::string, std::string> DictionaryDatabase::metadata(const std::string& path) {
    std::map<std::string, std::string> result;
    Database database(path, true);
    if (!database.isOpen()) return result;
    Statement query(database, "SELECT key, value FROM metadata");
    while (query.step()) result[query.text(0)] = query.text(1);
    return result;
}
