#pragma once

#include "../../Support/Json.hpp"

#include <optional>
#include <string>
#include <vector>

/// What two devices agree on: one JSON file holding every book's place and
/// group, and every word looked up with how it has fared in practice. Each
/// record carries when it last changed, and the newer one wins when both
/// devices have it. A word that was deleted stays as a tombstone, so the
/// other device deletes it too instead of bringing it back.
///
/// Times are strings of the form `2026-09-16T10:00:00Z`, so they sort as
/// text.
///
/// Both apps compile this file: the Kindle app directly, the iOS app through
/// Swift's C++ interop. Reading, writing and merging live only here, so the
/// two cannot disagree about them.
struct SyncDocument {
    static constexpr int version = 1;
    static const char* const fileName;

    struct BookRecord {
        std::string key;
        std::string title;
        std::string author;
        std::string language;
        /// Empty when the book is in no group.
        std::string group;
        /// Where the reader is: a place counts only with both chapter and
        /// fraction.
        std::optional<int> chapter;
        std::optional<double> fraction;
        std::string snippet;
        std::string updatedAt;

        bool operator==(const BookRecord& other) const;
        bool operator!=(const BookRecord& other) const { return !(*this == other); }
    };

    struct LookupRecord {
        std::string word;
        std::string sentence;
        std::string lemma;
        std::string formNote;
        std::string meaning;
        std::string language;
        /// The key of the book it was read in; empty when none.
        std::string book;
        bool guessed = false;
        double confidence = 0;
        std::string lookedUpAt;
        int correct = 0;
        int wrong = 0;
        std::string practicedAt;
        std::string updatedAt;
        bool deleted = false;

        std::string key() const { return word + '\x01' + sentence; }
        bool operator==(const LookupRecord& other) const;
        bool operator!=(const LookupRecord& other) const { return !(*this == other); }
    };

    std::vector<BookRecord> books;
    std::vector<LookupRecord> lookups;

    /// Records in a fixed order, so two documents with the same content
    /// compare and encode the same.
    SyncDocument sorted() const;
    bool operator==(const SyncDocument& other) const;
    bool operator!=(const SyncDocument& other) const { return !(*this == other); }

    /// Both documents' records, the newer of each pair. A tombstone beats a
    /// live record only when it is newer; a lookup made again after being
    /// deleted comes back.
    static SyncDocument merge(const SyncDocument& local, const SyncDocument& remote);

    Json toJson() const;
    std::string dump() const { return toJson().dump(); }
    static std::optional<SyncDocument> parse(const std::string& text);
};
