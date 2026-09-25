#pragma once

#include "../Domain/Dictionary/DictionaryPack.hpp"
#include "../Domain/Formats/DictionaryFormat.hpp"
#include "Database.hpp"

#include <string>
#include <vector>

/// The dictionaries the app searches: the bundled one, plus any the reader
/// has put in the dictionaries folder. Packs this app understands are taken
/// as they are; the other formats are converted into one on the way in,
/// which for a large dictionary takes a while.
class DictionaryPacks {
public:
    explicit DictionaryPacks(Database& database) : database_(database) {}

    std::vector<DictionaryPack> all();
    std::vector<DictionaryPack> enabled();
    void setEnabled(long long id, bool isEnabled);
    /// Removes the pack and its file. The bundled pack cannot be removed.
    void remove(const DictionaryPack& pack);

    /// Registers every dictionary in the folder that is not yet in the list.
    /// Returns how many were added; problems are described in `errors`.
    int addFromFolder(std::vector<std::string>& errors);
    /// Brings one dictionary from anywhere on disk into the folder — with the
    /// files that belong with it, for StarDict — and registers it. Counts and
    /// errors as `addFromFolder`; nothing is added if the file is already known.
    int addFile(const std::string& path, std::vector<std::string>& errors);

private:
    Database& database_;

    std::vector<DictionaryPack> query(const std::string& where);
    bool addNative(const std::string& fileName, std::string& error);
    bool convert(const DictionarySource& source, std::string& error);
    void insert(const DictionaryPack& pack);
};
