#include "WordContext.hpp"

#include "../../Support/Text.hpp"

#include <algorithm>

WordContext::WordContext(const std::string& text, const std::string& language) : text_(text) {
    const char* start = text_.c_str();
    const char* end = start + text_.size();
    for (const char* p = start; p < end; p = g_utf8_next_char(p)) {
        characterOffsets_.push_back(static_cast<int>(p - start));
    }
    characterOffsets_.push_back(static_cast<int>(text_.size()));

    attributes_.resize(characterOffsets_.size());
    PangoLanguage* pangoLanguage = language.empty()
        ? pango_language_get_default()
        : pango_language_from_string(language.c_str());
    pango_get_log_attrs(
        start, static_cast<int>(text_.size()), -1, pangoLanguage,
        attributes_.data(), static_cast<int>(attributes_.size()));
}

int WordContext::characterAt(int byteOffset) const {
    auto found = std::upper_bound(characterOffsets_.begin(), characterOffsets_.end(), byteOffset);
    if (found == characterOffsets_.begin()) return 0;
    return static_cast<int>(found - characterOffsets_.begin()) - 1;
}

std::optional<WordContext::Selection> WordContext::selectionAt(int byteOffset) const {
    if (byteOffset < 0 || byteOffset >= static_cast<int>(text_.size())) return std::nullopt;
    const int count = static_cast<int>(characterOffsets_.size()) - 1;
    int character = characterAt(byteOffset);

    int wordStart = character;
    while (wordStart > 0 && !attributes_[wordStart].is_word_start) --wordStart;
    if (!attributes_[wordStart].is_word_start) return std::nullopt;
    int wordEnd = wordStart + 1;
    while (wordEnd < count && !attributes_[wordEnd].is_word_end) ++wordEnd;
    // A tap in the gap after a word lands past its end: not a word.
    if (character >= wordEnd) return std::nullopt;

    int sentenceStart = wordStart;
    while (sentenceStart > 0 && !attributes_[sentenceStart].is_sentence_start) --sentenceStart;
    int sentenceEnd = wordEnd;
    while (sentenceEnd < count && !attributes_[sentenceEnd].is_sentence_end) ++sentenceEnd;

    int start = characterOffsets_[wordStart];
    int end = characterOffsets_[wordEnd];
    std::string sentence = text_.substr(characterOffsets_[sentenceStart], characterOffsets_[sentenceEnd] - characterOffsets_[sentenceStart]);
    return Selection{text_.substr(start, end - start), Text::trim(sentence), start, end};
}
