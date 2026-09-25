#include "WordNormalizer.hpp"

#include "../../Support/Text.hpp"

#include <glib.h>

namespace WordNormalizer {

static bool strippable(gunichar c) {
    return g_unichar_isspace(c) || g_unichar_ispunct(c);
}

std::string normalize(const std::string& word) {
    gchar* composed = g_utf8_normalize(word.c_str(), -1, G_NORMALIZE_NFC);
    std::string text = composed ? composed : word;
    g_free(composed);

    for (const char* quote : {"’", "ʼ", "＇"}) text = Text::replaceAll(text, quote, "'");
    for (const char* dash : {"‐", "‑", "‒", "–", "—"}) {
        text = Text::replaceAll(text, dash, "-");
    }
    text = Text::lower(text);

    // Trim punctuation, symbols and spaces from both ends, one character at a time.
    const char* start = text.c_str();
    const char* end = start + text.size();
    while (start < end && strippable(g_utf8_get_char(start))) start = g_utf8_next_char(start);
    while (end > start) {
        const char* previous = g_utf8_prev_char(end);
        if (!strippable(g_utf8_get_char(previous))) break;
        end = previous;
    }
    return std::string(start, end);
}

}  // namespace WordNormalizer
