#pragma once

#include "../../Domain/Dictionary/DictionaryPack.hpp"
#include "../../Services/Env.hpp"

#include <functional>
#include <string>
#include <vector>

/// The dictionaries the app searches: the bundled one, plus any the reader
/// has copied into the dictionaries folder.
class DictionariesFeature {
public:
    explicit DictionariesFeature(Env& env);

    const std::vector<DictionaryPack>& packs() const { return packs_; }
    void toggle(const DictionaryPack& pack);
    void remove(const DictionaryPack& pack);
    /// Registers what is new in the folder. Returns a message to show.
    std::string addFromFolder();
    /// Brings one dictionary in from anywhere on disk. Returns a message to show.
    std::string addFile(const std::string& path);

    std::function<void()> onChange;

private:
    Env& env_;
    std::vector<DictionaryPack> packs_;

    void reload();
    static std::string report(int added, const std::vector<std::string>& errors);
};
