#include "BookWords.hpp"

#include "../../Support/Text.hpp"
#include "../Dictionary/WordNormalizer.hpp"

#include <pango/pango.h>

#include <algorithm>
#include <map>

namespace BookWords {

namespace {

const size_t examplesPerForm = 3;
/// Words of context on each side of an example.
const size_t contextWords = 8;

/// A word's byte range in its paragraph.
struct Span {
    size_t start;
    size_t end;
};

std::vector<Span> wordsOf(const std::string& paragraph, PangoLanguage* language) {
    std::vector<size_t> offsets;
    const char* start = paragraph.c_str();
    const char* end = start + paragraph.size();
    for (const char* p = start; p < end; p = g_utf8_next_char(p)) offsets.push_back(p - start);
    offsets.push_back(paragraph.size());

    std::vector<PangoLogAttr> attributes(offsets.size());
    pango_get_log_attrs(start, static_cast<int>(paragraph.size()), -1, language,
        attributes.data(), static_cast<int>(attributes.size()));

    std::vector<Span> words;
    size_t count = offsets.size() - 1;
    for (size_t i = 0; i < count; ++i) {
        if (!attributes[i].is_word_start) continue;
        size_t j = i + 1;
        while (j < count && !attributes[j].is_word_end) ++j;
        words.push_back({offsets[i], offsets[j]});
        i = j - 1;
    }
    return words;
}

/// Letters, joined by apostrophes or hyphens: numbers have nothing to define.
bool isDefinable(const std::string& word) {
    bool letter = false;
    for (const char* p = word.c_str(); *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (g_unichar_isalpha(c)) letter = true;
        else if (!g_unichar_ismark(c) && c != '\'' && c != 0x2019 && c != 0x02BC && c != '-') return false;
    }
    return letter;
}

std::string exampleAt(const std::string& paragraph, const std::vector<Span>& words, size_t index) {
    size_t start = index <= contextWords ? 0 : words[index - contextWords].start;
    size_t last = index + contextWords;
    size_t end = last >= words.size() - 1 ? paragraph.size() : words[last].end;
    return std::string(start ? "…" : "") + Text::trim(paragraph.substr(start, end - start))
        + (end < paragraph.size() ? "…" : "");
}

}  // namespace

std::vector<BookWord> collect(const std::vector<std::string>& chapters, const std::string& language) {
    PangoLanguage* pangoLanguage = language.empty()
        ? pango_language_get_default()
        : pango_language_from_string(language.c_str());
    std::vector<BookWord> words;
    std::map<std::string, size_t> indexOf;
    for (const auto& chapter : chapters) {
        for (const auto& paragraph : Text::split(chapter, '\n')) {
            std::vector<Span> found = wordsOf(paragraph, pangoLanguage);
            for (size_t i = 0; i < found.size(); ++i) {
                std::string spelling = paragraph.substr(found[i].start, found[i].end - found[i].start);
                if (!isDefinable(spelling)) continue;
                std::string form = WordNormalizer::normalize(spelling);
                if (form.empty()) continue;
                auto known = indexOf.find(form);
                if (known == indexOf.end()) {
                    known = indexOf.emplace(form, words.size()).first;
                    words.push_back({form, spelling, {}});
                }
                auto& examples = words[known->second].examples;
                if (examples.size() >= examplesPerForm) continue;
                std::string example = exampleAt(paragraph, found, i);
                if (std::find(examples.begin(), examples.end(), example) == examples.end()) examples.push_back(example);
            }
        }
    }
    return words;
}

}  // namespace BookWords
