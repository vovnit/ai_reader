#include "BookSearch.hpp"

#include <glib.h>

namespace BookSearch {

namespace {

std::vector<gunichar> lowered(const std::string& text) {
    std::vector<gunichar> characters;
    const char* p = text.c_str();
    const char* end = p + text.size();
    for (; p < end; p = g_utf8_next_char(p)) characters.push_back(g_unichar_tolower(g_utf8_get_char(p)));
    return characters;
}

/// True when the text at `p` reads as `query`, letter for letter, ignoring
/// case. `matchEnd` receives where the match stops.
bool matches(const char* p, const char* end, const std::vector<gunichar>& query, const char** matchEnd) {
    for (gunichar wanted : query) {
        if (p >= end || g_unichar_tolower(g_utf8_get_char(p)) != wanted) return false;
        p = g_utf8_next_char(p);
    }
    *matchEnd = p;
    return true;
}

bool endsSentence(gunichar c) {
    return c == '.' || c == '!' || c == '?' || c == 0x2026 /* … */;
}

/// Where the sentence holding `offset` begins: after a paragraph break, or
/// after the space that follows a full stop.
int sentenceStart(const std::string& text, int offset, int floor) {
    const char* base = text.c_str();
    const char* p = base + offset;
    while (p > base + floor) {
        const char* previous = g_utf8_prev_char(p);
        gunichar c = g_utf8_get_char(previous);
        if (c == '\n') return static_cast<int>(p - base);
        if (g_unichar_isspace(c) && previous > base && endsSentence(g_utf8_get_char(g_utf8_prev_char(previous)))) {
            return static_cast<int>(p - base);
        }
        p = previous;
    }
    return static_cast<int>(p - base);
}

/// One past where the sentence holding `offset` ends: at a paragraph break,
/// or a full stop followed by space or the end of the text.
int sentenceEnd(const std::string& text, int offset, int ceiling) {
    const char* base = text.c_str();
    const char* end = base + ceiling;
    for (const char* p = base + offset; p < end; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (c == '\n') return static_cast<int>(p - base);
        if (!endsSentence(c)) continue;
        const char* next = g_utf8_next_char(p);
        // Closing quotes and brackets belong to the sentence.
        while (next < end && (g_utf8_get_char(next) == 0xBB || g_utf8_get_char(next) == '"' || g_utf8_get_char(next) == ')'
                              || g_utf8_get_char(next) == 0x201D)) {
            next = g_utf8_next_char(next);
        }
        if (next >= end || g_unichar_isspace(g_utf8_get_char(next))) return static_cast<int>(next - base);
    }
    return ceiling;
}

/// Moves a cut point to a character boundary, then to the nearest space on
/// the far side, so a cut never splits a word.
int cutBackward(const std::string& text, int at, int floor) {
    const char* base = text.c_str();
    const char* p = base + at;
    while (p > base + floor && (static_cast<unsigned char>(*p) & 0xC0) == 0x80) --p;
    while (p > base + floor && !g_unichar_isspace(g_utf8_get_char(p))) p = g_utf8_prev_char(p);
    return static_cast<int>(p - base);
}

int cutForward(const std::string& text, int at, int ceiling) {
    const char* base = text.c_str();
    const char* p = base + at;
    while (p < base + ceiling && (static_cast<unsigned char>(*p) & 0xC0) == 0x80) ++p;
    while (p < base + ceiling && !g_unichar_isspace(g_utf8_get_char(p))) p = g_utf8_next_char(p);
    return static_cast<int>(p - base);
}

/// The stretch of text an excerpt covers, and whether either end was cut.
struct Cut {
    int from;
    int to;
    bool front;
    bool back;
};

Cut cut(const std::string& text, int start, int end, int reach) {
    int size = static_cast<int>(text.size());
    Cut cut;
    cut.from = sentenceStart(text, start, std::max(0, start - reach));
    cut.to = sentenceEnd(text, end, std::min(size, end + reach));
    cut.front = cut.from > 0 && text[cut.from - 1] != '\n' && start - cut.from >= reach;
    cut.back = cut.to < size && text[cut.to] != '\n' && cut.to - end >= reach;
    if (cut.front) cut.from = cutForward(text, cut.from, start);
    if (cut.back) cut.to = cutBackward(text, cut.to, end);
    return cut;
}

}  // namespace

std::vector<SearchHit> find(const std::string& text, const std::string& query, int chapter, int limit) {
    std::vector<SearchHit> hits;
    std::vector<gunichar> wanted = lowered(query);
    if (wanted.empty() || limit <= 0) return hits;

    const char* base = text.c_str();
    const char* end = base + text.size();
    const char* p = base;
    // Where the last excerpt ended: a second match in the same sentence is
    // the same passage, listed once.
    int covered = 0;
    while (p < end && static_cast<int>(hits.size()) < limit) {
        const char* matchEnd = nullptr;
        if (!matches(p, end, wanted, &matchEnd)) {
            p = g_utf8_next_char(p);
            continue;
        }
        int start = static_cast<int>(p - base);
        p = matchEnd;
        if (start < covered) continue;
        SearchHit hit = excerpt(text, start, static_cast<int>(matchEnd - base));
        hit.chapter = chapter;
        hits.push_back(hit);
        covered = cut(text, start, static_cast<int>(matchEnd - base), reach).to;
    }
    return hits;
}

SearchHit excerpt(const std::string& text, int start, int end, int reach) {
    Cut range = cut(text, start, end, reach);
    SearchHit hit;
    hit.offset = start;
    hit.excerpt = (range.front ? "…" : "") + text.substr(range.from, range.to - range.from) + (range.back ? "…" : "");
    int shift = range.front ? static_cast<int>(std::string("…").size()) : 0;
    hit.matchStart = start - range.from + shift;
    hit.matchEnd = end - range.from + shift;
    // Trim the whitespace a sentence boundary leaves behind.
    while (hit.matchStart > 0 && (hit.excerpt[0] == ' ' || hit.excerpt[0] == '\t')) {
        hit.excerpt.erase(0, 1);
        --hit.matchStart;
        --hit.matchEnd;
    }
    while (!hit.excerpt.empty() && hit.excerpt.back() == ' ') hit.excerpt.pop_back();
    return hit;
}

}  // namespace BookSearch
