#include "ReadingPlace.hpp"

#include "../../Support/Text.hpp"

#include <glib.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

/// `text` with whitespace collapsed and illustration placeholders dropped,
/// and for each byte of the result the byte offset it came from.
struct Normalized {
    std::string text;
    std::vector<int> map;
};

Normalized normalize(const std::string& text) {
    Normalized out;
    bool pendingSpace = false;
    const char* base = text.c_str();
    const char* end = base + text.size();
    for (const char* p = base; p < end;) {
        gunichar c = g_utf8_get_char(p);
        const char* next = g_utf8_next_char(p);
        if (c == 0xFFFC) { p = next; continue; }
        if (g_unichar_isspace(c)) {
            pendingSpace = !out.text.empty();
            p = next;
            continue;
        }
        if (pendingSpace) {
            out.text += ' ';
            out.map.push_back(static_cast<int>(p - base));
            pendingSpace = false;
        }
        for (const char* q = p; q < next; ++q) {
            out.text += *q;
            out.map.push_back(static_cast<int>(p - base));
        }
        p = next;
    }
    return out;
}

}  // namespace

ReadingPlace ReadingPlace::at(int chapter, const std::string& chapterText, int offset) {
    int size = static_cast<int>(chapterText.size());
    int start = std::min(std::max(offset, 0), size);
    ReadingPlace place;
    place.chapter = chapter;
    place.fraction = size > 0 ? static_cast<double>(start) / size : 0;

    std::string ahead = chapterText.substr(start, std::min(size - start, snippetLength * 3));
    std::string normalized = normalize(ahead).text;
    int end = std::min(static_cast<int>(normalized.size()), snippetLength);
    // Back off a cut inside a character, then to a space so the snippet is
    // whole words, unless that would leave nothing.
    while (end > 0 && end < static_cast<int>(normalized.size()) && (static_cast<unsigned char>(normalized[end]) & 0xC0) == 0x80) --end;
    if (end < static_cast<int>(normalized.size())) {
        int cut = end;
        while (cut > 0 && normalized[cut - 1] != ' ') --cut;
        if (cut > 0) end = cut;
    }
    place.snippet = Text::trim(normalized.substr(0, end));
    return place;
}

int ReadingPlace::resolve(const std::string& chapterText) const {
    double clamped = std::min(std::max(fraction, 0.0), 1.0);
    auto byFraction = [&] {
        int offset = static_cast<int>(std::floor(chapterText.size() * clamped));
        // Never land inside a character.
        while (offset > 0 && offset < static_cast<int>(chapterText.size())
               && (static_cast<unsigned char>(chapterText[offset]) & 0xC0) == 0x80) --offset;
        return offset;
    };
    if (snippet.empty()) return byFraction();

    Normalized normalized = normalize(chapterText);
    double expected = normalized.text.size() * clamped;
    // The nearest occurrence to where the fraction says it should be.
    std::optional<size_t> best;
    for (size_t found = normalized.text.find(snippet); found != std::string::npos; found = normalized.text.find(snippet, found + 1)) {
        if (!best || std::fabs(static_cast<double>(found) - expected) < std::fabs(static_cast<double>(*best) - expected)) best = found;
    }
    if (!best || *best >= normalized.map.size()) return byFraction();
    return normalized.map[*best];
}
