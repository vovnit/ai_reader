#pragma once

#include <pango/pango.h>

#include <optional>
#include <string>
#include <vector>

/// Tokenizes a chapter once so a tap position can be turned into the word that
/// was touched together with the sentence it sits in.
class WordContext {
public:
    struct Selection {
        std::string word;
        std::string sentence;
        /// Byte range of the word in the chapter text.
        int start = 0;
        int end = 0;
    };

    WordContext() = default;
    WordContext(const std::string& text, const std::string& language);

    std::optional<Selection> selectionAt(int byteOffset) const;

private:
    std::string text_;
    std::vector<PangoLogAttr> attributes_;
    /// Byte offset of each character, plus one past the end.
    std::vector<int> characterOffsets_;

    int characterAt(int byteOffset) const;
};
