#include "DictionaryPackWriter.hpp"

#include "../Domain/Dictionary/WordNormalizer.hpp"
#include "../Support/Files.hpp"
#include "Support/Json.hpp"
#include "../Support/Text.hpp"

#include <zlib.h>

DictionaryPackWriter::DictionaryPackWriter(const std::string& path) {
    Files::remove(path);
    database_ = std::make_unique<Database>(path);
    if (!database_->isOpen()) return;
    bool ok = database_->exec("CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL)")
        && database_->exec("CREATE TABLE lemmas (id INTEGER PRIMARY KEY, word TEXT NOT NULL UNIQUE)")
        && database_->exec(
            "CREATE TABLE forms ("
            " normalized_form TEXT NOT NULL, ordinal INTEGER NOT NULL, form TEXT NOT NULL,"
            " lemma_id INTEGER NOT NULL, part_of_speech TEXT NOT NULL, gender TEXT, number TEXT,"
            " verb_info TEXT NOT NULL, PRIMARY KEY (normalized_form, ordinal)) WITHOUT ROWID")
        && database_->exec(
            "CREATE TABLE entries ("
            " lemma_id INTEGER NOT NULL, ordinal INTEGER NOT NULL, payload BLOB NOT NULL,"
            " PRIMARY KEY (lemma_id, ordinal)) WITHOUT ROWID")
        && database_->exec("BEGIN");
    if (!ok) database_.reset();
}

static std::string deflate(const std::string& json) {
    uLongf size = compressBound(static_cast<uLong>(json.size()));
    std::string out(size, '\0');
    if (compress(reinterpret_cast<Bytef*>(&out[0]), &size, reinterpret_cast<const Bytef*>(json.data()),
                 static_cast<uLong>(json.size())) != Z_OK) {
        return json;
    }
    out.resize(size);
    return out;
}

void DictionaryPackWriter::add(const DictionaryImportEntry& entry) {
    if (!isOpen()) return;
    std::string word = WordNormalizer::normalize(entry.headword);
    Json glosses = Json::array();
    for (const auto& sense : entry.senses) {
        std::string trimmed = Text::trim(sense);
        if (!trimmed.empty()) glosses.push(trimmed);
    }
    if (word.empty() || glosses.size() == 0) return;

    long long id;
    auto known = lemmaIds_.find(word);
    if (known != lemmaIds_.end()) {
        id = known->second;
    } else {
        id = static_cast<long long>(lemmaIds_.size()) + 1;
        lemmaIds_[word] = id;
        Statement insert(*database_, "INSERT INTO lemmas (id, word) VALUES (?, ?)");
        insert.bind(1, id).bind(2, word).run();
    }
    int ordinal = ordinals_[id]++;

    // The article shape `DictionaryDatabase` decodes.
    Json definition = Json::object();
    definition.set("glosses", glosses);
    Json article = Json::object();
    article.set("definitions", Json(std::vector<Json>{definition}));
    if (!entry.partOfSpeech.empty()) article.set("part_of_speech", entry.partOfSpeech);

    Statement insert(*database_, "INSERT INTO entries (lemma_id, ordinal, payload) VALUES (?, ?, ?)");
    insert.bind(1, id).bind(2, ordinal).bindBlob(3, deflate(article.dump())).run();
    ++entryCount_;
}

bool DictionaryPackWriter::finish(const std::string& targetLanguage, const std::string& definitionLanguage) {
    if (!isOpen() || entryCount_ == 0) return false;
    std::map<std::string, std::string> metadata = {
        {"schema_version", "2"},
        {"mode", "converted"},
        {"payload_encoding", "json+zlib"},
        {"lemma_count", std::to_string(lemmaIds_.size())},
        {"form_count", "0"},
        {"definition_entry_count", std::to_string(entryCount_)},
        {"target_language", targetLanguage},
        {"definition_language", definitionLanguage},
    };
    Statement insert(*database_, "INSERT INTO metadata (key, value) VALUES (?, ?)");
    for (const auto& pair : metadata) {
        if (pair.second.empty()) continue;
        insert.reset();
        insert.bind(1, pair.first).bind(2, pair.second).run();
    }
    return database_->exec("COMMIT");
}
