#include "SyncStore.hpp"

#include "../Domain/Books/BookKey.hpp"

#include <map>

namespace SyncStore {

namespace {

/// The file keeps a place as three plain fields; it counts only whole.
std::optional<ReadingPlace> placeOf(const SyncDocument::BookRecord& record) {
    if (!record.chapter || !record.fraction) return std::nullopt;
    return ReadingPlace{*record.chapter, *record.fraction, record.snippet};
}

std::string lookupUpdatedAt(const Lookup& lookup, const std::map<long long, Card>& cards) {
    auto card = cards.find(lookup.id);
    if (card == cards.end() || card->second.practicedAt.empty()) return lookup.lookedUpAt;
    return std::max(lookup.lookedUpAt, card->second.practicedAt);
}

}  // namespace

SyncDocument exportAll(Env& env) {
    SyncDocument document;
    std::map<long long, std::string> keys;
    std::map<long long, std::string> groups;
    for (const auto& group : env.groups.all()) groups[group.id] = group.name;
    for (const auto& book : env.library.all()) {
        SyncDocument::BookRecord record;
        record.key = BookKey::make(book.title, book.author);
        record.title = book.title;
        record.author = book.author;
        record.language = book.language;
        if (book.groupId) record.group = groups[book.groupId];
        if (book.place) {
            record.chapter = book.place->chapter;
            record.fraction = book.place->fraction;
            record.snippet = book.place->snippet;
        }
        record.updatedAt = book.updatedAt;
        keys[book.id] = record.key;
        document.books.push_back(record);
    }

    std::map<long long, Card> cards;
    for (const auto& card : env.cards.all()) cards[card.lookupId] = card;
    for (const auto& lookup : env.lookups.all()) {
        SyncDocument::LookupRecord record;
        record.word = lookup.word;
        record.sentence = lookup.sentence;
        record.lemma = lookup.lemma;
        record.formNote = lookup.formNote;
        record.meaning = lookup.meaning;
        record.language = lookup.language;
        if (lookup.bookId) record.book = keys[lookup.bookId];
        record.guessed = lookup.guessed;
        record.confidence = lookup.confidence;
        record.lookedUpAt = lookup.lookedUpAt;
        auto card = cards.find(lookup.id);
        if (card != cards.end()) {
            record.correct = card->second.correct;
            record.wrong = card->second.wrong;
            record.practicedAt = card->second.practicedAt;
        }
        record.updatedAt = lookupUpdatedAt(lookup, cards);
        document.lookups.push_back(record);
    }
    for (const auto& tombstone : env.lookups.tombstones()) {
        SyncDocument::LookupRecord record;
        record.word = tombstone.word;
        record.sentence = tombstone.sentence;
        record.updatedAt = tombstone.deletedAt;
        record.deleted = true;
        document.lookups.push_back(record);
    }
    return document.sorted();
}

Applied apply(Env& env, const SyncDocument& merged) {
    SyncDocument local = exportAll(env);
    Applied applied;

    std::map<std::string, SyncDocument::BookRecord> localBooks;
    for (const auto& record : local.books) localBooks[record.key] = record;
    std::map<std::string, Book> books;
    for (const auto& book : env.library.all()) books[BookKey::make(book.title, book.author)] = book;

    for (const auto& record : merged.books) {
        auto book = books.find(record.key);
        auto known = localBooks.find(record.key);
        if (book == books.end() || (known != localBooks.end() && known->second == record)) continue;
        long long groupId = record.group.empty() ? 0 : env.groups.named(record.group);
        env.library.assignGroup(book->second.id, groupId, record.updatedAt);
        // A place from elsewhere is worked out into an offset when the book
        // is next opened; the same place again is left alone.
        auto place = placeOf(record);
        if (place && place != book->second.place) {
            env.library.savePendingPlace(book->second.id, *place, record.updatedAt);
        }
        ++applied.books;
    }

    std::map<std::string, SyncDocument::LookupRecord> localLookups;
    for (const auto& record : local.lookups) localLookups[record.key()] = record;
    std::map<std::string, long long> existing;
    for (const auto& lookup : env.lookups.all()) existing[lookup.word + '\x01' + lookup.sentence] = lookup.bookId;

    for (const auto& record : merged.lookups) {
        auto known = localLookups.find(record.key());
        if (known != localLookups.end() && known->second == record) continue;
        ++applied.lookups;
        if (record.deleted) {
            env.lookups.removeNamed(record.word, record.sentence);
            env.lookups.addTombstone({record.word, record.sentence, record.updatedAt});
            continue;
        }
        env.lookups.removeTombstone(record.word, record.sentence);
        Lookup lookup;
        lookup.word = record.word;
        lookup.sentence = record.sentence;
        lookup.lemma = record.lemma;
        lookup.formNote = record.formNote;
        lookup.meaning = record.meaning;
        lookup.language = record.language;
        // A book this device does not have leaves the lookup attached to
        // whatever it was attached to here.
        auto book = books.find(record.book);
        auto here = existing.find(record.key());
        lookup.bookId = book != books.end() ? book->second.id : here != existing.end() ? here->second : 0;
        lookup.guessed = record.guessed;
        lookup.confidence = record.confidence;
        lookup.lookedUpAt = record.lookedUpAt.empty() ? record.updatedAt : record.lookedUpAt;
        long long id = env.lookups.upsert(lookup);
        if (id && (record.correct + record.wrong > 0 || !record.practicedAt.empty())) {
            env.cards.set(id, record.correct, record.wrong,
                          record.practicedAt.empty() ? lookup.lookedUpAt : record.practicedAt);
        }
    }

    // A group nothing is left in has been dissolved somewhere.
    for (const auto& group : env.groups.all()) {
        if (env.library.inGroup(group.id).empty()) env.groups.remove(group.id);
    }
    return applied;
}

}  // namespace SyncStore
