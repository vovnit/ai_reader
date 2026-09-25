#pragma once

#include "../Domain/AI/WordExplanation.hpp"
#include "Database.hpp"

#include <optional>
#include <vector>

/// Remembers explanations so the same word in the same sentence is only paid
/// for once — and keeps the list of every word met so far.
class LookupCache {
public:
    explicit LookupCache(Database& database) : database_(database) {}

    std::optional<WordExplanation> cached(const LookupContext& context);
    void save(const LookupContext& context, const WordExplanation& explanation);
    /// Forgets a lookup, and remembers that it was forgotten, so a sync does
    /// not bring it back from another device.
    void remove(long long id);
    /// Every lookup, newest first; `bookId` narrows it to one book.
    std::vector<Lookup> all(long long bookId = 0);

    /// A lookup that was deleted, by name.
    struct Tombstone {
        std::string word;
        std::string sentence;
        std::string deletedAt;
    };
    std::vector<Tombstone> tombstones();
    void addTombstone(const Tombstone& tombstone);
    void removeTombstone(const std::string& word, const std::string& sentence);
    /// Writes a lookup from another device: over the one here with the same
    /// word and sentence, or as a new row. Returns its id.
    long long upsert(const Lookup& lookup);
    /// Deletes by name; true when there was one.
    bool removeNamed(const std::string& word, const std::string& sentence);

private:
    Database& database_;
};
