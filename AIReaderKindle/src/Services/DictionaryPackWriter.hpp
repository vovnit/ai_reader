#pragma once

#include "../Domain/Formats/DictionaryFormat.hpp"
#include "Database.hpp"

#include <map>
#include <memory>
#include <string>

/// Writes the schema version 2 pack the app reads, one article at a time.
///
/// Converted dictionaries carry no inflection tables, so `forms` is left empty
/// and lookups fall back to matching the headword. That is why headwords are
/// stored normalized: the reader taps “Paris” and the query arrives as “paris”.
class DictionaryPackWriter {
public:
    explicit DictionaryPackWriter(const std::string& path);

    bool isOpen() const { return database_ && database_->isOpen(); }
    void add(const DictionaryImportEntry& entry);
    /// Writes the metadata. False when no entry could be read.
    bool finish(const std::string& targetLanguage, const std::string& definitionLanguage);
    int entryCount() const { return entryCount_; }

private:
    std::unique_ptr<Database> database_;
    std::map<std::string, long long> lemmaIds_;
    std::map<long long, int> ordinals_;
    int entryCount_ = 0;
};
