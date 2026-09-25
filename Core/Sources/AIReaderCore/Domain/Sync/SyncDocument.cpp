#include "SyncDocument.hpp"

#include <algorithm>
#include <map>

const char* const SyncDocument::fileName = "aireader-sync.json";

bool SyncDocument::BookRecord::operator==(const BookRecord& other) const {
    return key == other.key && title == other.title && author == other.author && language == other.language
        && group == other.group && chapter == other.chapter && fraction == other.fraction && snippet == other.snippet
        && updatedAt == other.updatedAt;
}

bool SyncDocument::LookupRecord::operator==(const LookupRecord& other) const {
    return word == other.word && sentence == other.sentence && lemma == other.lemma && formNote == other.formNote
        && meaning == other.meaning && language == other.language && book == other.book && guessed == other.guessed
        && confidence == other.confidence && lookedUpAt == other.lookedUpAt && correct == other.correct
        && wrong == other.wrong && practicedAt == other.practicedAt && updatedAt == other.updatedAt
        && deleted == other.deleted;
}

SyncDocument SyncDocument::sorted() const {
    SyncDocument copy = *this;
    std::sort(copy.books.begin(), copy.books.end(), [](const BookRecord& a, const BookRecord& b) { return a.key < b.key; });
    std::sort(copy.lookups.begin(), copy.lookups.end(), [](const LookupRecord& a, const LookupRecord& b) { return a.key() < b.key(); });
    return copy;
}

bool SyncDocument::operator==(const SyncDocument& other) const {
    return books == other.books && lookups == other.lookups;
}

SyncDocument SyncDocument::merge(const SyncDocument& local, const SyncDocument& remote) {
    std::map<std::string, BookRecord> books;
    for (const auto* side : {&remote.books, &local.books}) {
        for (const auto& record : *side) {
            auto known = books.find(record.key);
            if (known != books.end() && known->second.updatedAt >= record.updatedAt) continue;
            books[record.key] = record;
        }
    }

    std::map<std::string, LookupRecord> lookups;
    for (const auto* side : {&remote.lookups, &local.lookups}) {
        for (const auto& record : *side) {
            auto known = lookups.find(record.key());
            if (known == lookups.end()) {
                lookups[record.key()] = record;
                continue;
            }
            LookupRecord newest = known->second;
            LookupRecord older = record;
            if (record.updatedAt > newest.updatedAt) std::swap(newest, older);
            // A device without the book still knows the word; keep which
            // book the other device met it in.
            if (newest.book.empty()) newest.book = older.book;
            known->second = newest;
        }
    }

    SyncDocument merged;
    for (const auto& entry : books) merged.books.push_back(entry.second);
    for (const auto& entry : lookups) {
        LookupRecord record = entry.second;
        // A tombstone is only a name and a time, however it was made, so
        // two devices' copies of one compare equal.
        if (record.deleted) record = LookupRecord{record.word, record.sentence, "", "", "", "", "", false, 0, "", 0, 0, "", record.updatedAt, true};
        merged.lookups.push_back(record);
    }
    return merged.sorted();
}

// MARK: - JSON

namespace {

/// Optional strings travel as null, the way the iOS app writes them.
Json text(const std::string& value) {
    return value.empty() ? Json(nullptr) : Json(value);
}

std::string text(const Json& value) {
    return value.isString() ? value.string() : "";
}

}  // namespace

Json SyncDocument::toJson() const {
    SyncDocument ordered = sorted();
    Json books = Json::array();
    for (const auto& book : ordered.books) {
        Json record = Json::object();
        record.set("author", text(book.author));
        record.set("chapter", book.chapter ? Json(*book.chapter) : Json(nullptr));
        record.set("fraction", book.fraction ? Json(*book.fraction) : Json(nullptr));
        record.set("group", text(book.group));
        record.set("key", book.key);
        record.set("language", text(book.language));
        record.set("snippet", text(book.snippet));
        record.set("title", book.title);
        record.set("updatedAt", book.updatedAt);
        books.push(record);
    }
    Json lookups = Json::array();
    for (const auto& lookup : ordered.lookups) {
        Json record = Json::object();
        record.set("book", text(lookup.book));
        record.set("confidence", lookup.confidence);
        record.set("correct", lookup.correct);
        record.set("deleted", lookup.deleted);
        record.set("formNote", lookup.formNote);
        record.set("guessed", lookup.guessed);
        record.set("language", text(lookup.language));
        record.set("lemma", lookup.lemma);
        record.set("lookedUpAt", lookup.lookedUpAt);
        record.set("meaning", lookup.meaning);
        record.set("practicedAt", text(lookup.practicedAt));
        record.set("sentence", lookup.sentence);
        record.set("updatedAt", lookup.updatedAt);
        record.set("word", lookup.word);
        record.set("wrong", lookup.wrong);
        lookups.push(record);
    }
    Json document = Json::object();
    document.set("books", books);
    document.set("lookups", lookups);
    document.set("version", version);
    return document;
}

std::optional<SyncDocument> SyncDocument::parse(const std::string& source) {
    auto json = Json::parse(source);
    if (!json || !json->isObject()) return std::nullopt;
    SyncDocument document;
    for (const auto& item : json->at("books").items()) {
        BookRecord record;
        record.key = text(item.at("key"));
        record.title = text(item.at("title"));
        record.author = text(item.at("author"));
        record.language = text(item.at("language"));
        record.group = text(item.at("group"));
        if (item.at("chapter").isNumber()) record.chapter = static_cast<int>(item.at("chapter").number());
        if (item.at("fraction").isNumber()) record.fraction = item.at("fraction").number();
        record.snippet = text(item.at("snippet"));
        record.updatedAt = text(item.at("updatedAt"));
        if (!record.key.empty()) document.books.push_back(record);
    }
    for (const auto& item : json->at("lookups").items()) {
        LookupRecord record;
        record.word = text(item.at("word"));
        record.sentence = text(item.at("sentence"));
        record.lemma = text(item.at("lemma"));
        record.formNote = text(item.at("formNote"));
        record.meaning = text(item.at("meaning"));
        record.language = text(item.at("language"));
        record.book = text(item.at("book"));
        record.guessed = item.at("guessed").boolean();
        record.confidence = item.at("confidence").number();
        record.lookedUpAt = text(item.at("lookedUpAt"));
        record.correct = static_cast<int>(item.at("correct").number());
        record.wrong = static_cast<int>(item.at("wrong").number());
        record.practicedAt = text(item.at("practicedAt"));
        record.updatedAt = text(item.at("updatedAt"));
        record.deleted = item.at("deleted").boolean();
        if (!record.word.empty()) document.lookups.push_back(record);
    }
    return document;
}
