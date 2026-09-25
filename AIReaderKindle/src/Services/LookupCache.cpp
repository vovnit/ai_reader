#include "LookupCache.hpp"

#include "Migrations.hpp"

std::optional<WordExplanation> LookupCache::cached(const LookupContext& context) {
    Statement query(database_,
        "SELECT lemma, formNote, meaning, guessed, confidence FROM lookups WHERE word = ? AND sentence = ?");
    query.bind(1, context.word).bind(2, context.sentence);
    if (!query.step()) return std::nullopt;
    return WordExplanation{query.text(0), query.text(1), query.text(2), query.integer(3) != 0, query.real(4)};
}

void LookupCache::save(const LookupContext& context, const WordExplanation& explanation) {
    Statement remove(database_, "DELETE FROM lookups WHERE word = ? AND sentence = ?");
    remove.bind(1, context.word).bind(2, context.sentence).run();
    removeTombstone(context.word, context.sentence);

    Statement insert(database_,
        "INSERT INTO lookups (word, sentence, lemma, formNote, meaning, language, bookID, guessed, confidence, lookedUpAt)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    insert.bind(1, context.word).bind(2, context.sentence).bind(3, explanation.lemma)
        .bind(4, explanation.formNote).bind(5, explanation.meaning);
    if (context.language.empty()) insert.bindNull(6); else insert.bind(6, context.language);
    if (context.bookId) insert.bind(7, context.bookId); else insert.bindNull(7);
    insert.bind(8, explanation.guessed ? 1 : 0).bind(9, explanation.confidence).bind(10, Migrations::now());
    insert.run();
}

void LookupCache::remove(long long id) {
    Statement named(database_, "SELECT word, sentence FROM lookups WHERE id = ?");
    named.bind(1, id);
    if (named.step()) addTombstone({named.text(0), named.text(1), Migrations::now()});
    Statement remove(database_, "DELETE FROM lookups WHERE id = ?");
    remove.bind(1, id).run();
}

std::vector<LookupCache::Tombstone> LookupCache::tombstones() {
    std::vector<Tombstone> list;
    Statement query(database_, "SELECT word, sentence, deletedAt FROM lookupTombstones");
    while (query.step()) list.push_back({query.text(0), query.text(1), query.text(2)});
    return list;
}

void LookupCache::addTombstone(const Tombstone& tombstone) {
    Statement insert(database_, "INSERT OR REPLACE INTO lookupTombstones (word, sentence, deletedAt) VALUES (?, ?, ?)");
    insert.bind(1, tombstone.word).bind(2, tombstone.sentence).bind(3, tombstone.deletedAt).run();
}

void LookupCache::removeTombstone(const std::string& word, const std::string& sentence) {
    Statement remove(database_, "DELETE FROM lookupTombstones WHERE word = ? AND sentence = ?");
    remove.bind(1, word).bind(2, sentence).run();
}

long long LookupCache::upsert(const Lookup& lookup) {
    Statement existing(database_, "SELECT id FROM lookups WHERE word = ? AND sentence = ?");
    existing.bind(1, lookup.word).bind(2, lookup.sentence);
    if (existing.step()) {
        long long id = existing.integer(0);
        Statement update(database_,
            "UPDATE lookups SET lemma = ?, formNote = ?, meaning = ?, language = ?, bookID = ?, guessed = ?,"
            " confidence = ?, lookedUpAt = ? WHERE id = ?");
        update.bind(1, lookup.lemma).bind(2, lookup.formNote).bind(3, lookup.meaning);
        if (lookup.language.empty()) update.bindNull(4); else update.bind(4, lookup.language);
        if (lookup.bookId) update.bind(5, lookup.bookId); else update.bindNull(5);
        update.bind(6, lookup.guessed ? 1 : 0).bind(7, lookup.confidence).bind(8, lookup.lookedUpAt).bind(9, id).run();
        return id;
    }
    Statement insert(database_,
        "INSERT INTO lookups (word, sentence, lemma, formNote, meaning, language, bookID, guessed, confidence, lookedUpAt)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    insert.bind(1, lookup.word).bind(2, lookup.sentence).bind(3, lookup.lemma).bind(4, lookup.formNote).bind(5, lookup.meaning);
    if (lookup.language.empty()) insert.bindNull(6); else insert.bind(6, lookup.language);
    if (lookup.bookId) insert.bind(7, lookup.bookId); else insert.bindNull(7);
    insert.bind(8, lookup.guessed ? 1 : 0).bind(9, lookup.confidence).bind(10, lookup.lookedUpAt);
    return insert.run() ? database_.lastInsertId() : 0;
}

bool LookupCache::removeNamed(const std::string& word, const std::string& sentence) {
    Statement existing(database_, "SELECT id FROM lookups WHERE word = ? AND sentence = ?");
    existing.bind(1, word).bind(2, sentence);
    if (!existing.step()) return false;
    Statement remove(database_, "DELETE FROM lookups WHERE word = ? AND sentence = ?");
    remove.bind(1, word).bind(2, sentence).run();
    return true;
}

std::vector<Lookup> LookupCache::all(long long bookId) {
    std::string sql =
        "SELECT id, word, sentence, lemma, formNote, meaning, language, bookID, guessed, confidence, lookedUpAt"
        " FROM lookups";
    if (bookId) sql += " WHERE bookID = ?";
    sql += " ORDER BY lookedUpAt DESC, id DESC";

    Statement query(database_, sql);
    if (bookId) query.bind(1, bookId);
    std::vector<Lookup> lookups;
    while (query.step()) {
        Lookup lookup;
        lookup.id = query.integer(0);
        lookup.word = query.text(1);
        lookup.sentence = query.text(2);
        lookup.lemma = query.text(3);
        lookup.formNote = query.text(4);
        lookup.meaning = query.text(5);
        lookup.language = query.text(6);
        lookup.bookId = query.integer(7);
        lookup.guessed = query.integer(8) != 0;
        lookup.confidence = query.real(9);
        lookup.lookedUpAt = query.text(10);
        lookups.push_back(lookup);
    }
    return lookups;
}
