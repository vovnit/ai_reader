#include "Card.hpp"

#include <glib.h>

#include <algorithm>

namespace {

bool letterBefore(const std::string& text, size_t at) {
    if (at == 0) return false;
    return g_unichar_isalnum(g_utf8_get_char(g_utf8_prev_char(text.c_str() + at)));
}

bool letterAt(const std::string& text, size_t at) {
    if (at >= text.size()) return false;
    return g_unichar_isalnum(g_utf8_get_char(text.c_str() + at));
}

}  // namespace

Card Card::fromLookup(const Lookup& lookup) {
    Card card;
    card.lookupId = lookup.id;
    card.front = lookup.word;
    if (lookup.lemma != lookup.word) card.lemma = lookup.lemma;
    card.back = lookup.meaning;
    card.sentence = lookup.sentence;
    card.example = blank(lookup.sentence, lookup.word);
    return card;
}

std::string Card::blank(const std::string& sentence, const std::string& word) {
    if (word.empty()) return sentence;
    static const std::string gap = "____";
    std::string out = sentence;
    size_t at = 0;
    while ((at = out.find(word, at)) != std::string::npos) {
        // "a" inside "avait" is not the word.
        if (letterBefore(out, at) || letterAt(out, at + word.size())) {
            ++at;
            continue;
        }
        out.replace(at, word.size(), gap);
        at += gap.size();
    }
    return out;
}

std::vector<Card> Card::due(std::vector<Card> cards, size_t count) {
    std::stable_sort(cards.begin(), cards.end(), [](const Card& a, const Card& b) {
        if (a.practicedAt.empty() != b.practicedAt.empty()) return a.practicedAt.empty();
        int missedA = a.wrong - a.correct, missedB = b.wrong - b.correct;
        if (missedA != missedB) return missedA > missedB;
        return a.practicedAt < b.practicedAt;
    });
    if (cards.size() > count) cards.resize(count);
    return cards;
}
