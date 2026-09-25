#pragma once

#include "../Domain/Dictionary/DictionaryLookup.hpp"
#include "../Domain/Dictionary/DictionaryPack.hpp"
#include "Database.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/// Read-only access to the dictionary packs. Safe to call from a worker
/// thread; connections are opened once and kept.
class DictionaryDatabase {
public:
    static DictionaryDatabase& shared();

    /// The path a pack's file is at.
    static std::string path(const DictionaryPack& pack);

    /// Searches every pack given and merges what they say about the word.
    DictionaryLookup lookup(const std::string& word, const std::vector<DictionaryPack>& packs);
    /// The articles filed under exactly this headword: the entry a lemma
    /// came from. A form of another word brings nothing.
    std::vector<DictionaryLookup::Article> articlesFor(const std::string& lemma, const std::vector<DictionaryPack>& packs);

    /// The `metadata` table of a pack file, or empty if it is not one.
    static std::map<std::string, std::string> metadata(const std::string& path);

private:
    std::mutex mutex_;
    std::map<std::string, std::unique_ptr<Database>> connections_;

    Database* connection(const std::string& path);
    static void search(Database& database, const std::string& normalized, const std::string& source, DictionaryLookup& result);
};
