#pragma once

#include <string>

/// A dictionary the app can search. One ships with the app; others are added
/// by the reader.
struct DictionaryPack {
    long long id = 0;
    std::string name;
    /// File name under the dictionaries folder, or empty for the bundled pack.
    std::string fileName;
    std::string targetLanguage;
    std::string definitionLanguage;
    bool isEnabled = true;
    std::string addedAt;

    bool isBundled() const { return fileName.empty(); }

    /// "fr → ru" when the pack says what it holds.
    std::string languages() const {
        if (targetLanguage.empty() || definitionLanguage.empty()) return "";
        return targetLanguage + " → " + definitionLanguage;
    }
};
