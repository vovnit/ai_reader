#pragma once

#include "../AI/WordExplanation.hpp"

#include <string>
#include <vector>

/// A flash card made from a lookup, the way Anki holds one: the word on the
/// front, what it meant there on the back, and the sentence it was met in
/// with the word blanked out. The practice record says how it has fared.
struct Card {
    long long lookupId = 0;
    /// The form met in the text.
    std::string front;
    /// Its dictionary form, when that differs from the front.
    std::string lemma;
    /// The meaning that fit the sentence.
    std::string back;
    /// The sentence it was met in, and the same with the word blanked.
    std::string sentence;
    std::string example;
    int correct = 0;
    int wrong = 0;
    /// When it was last practised, in the tables' ISO form; empty until then.
    std::string practicedAt;

    static Card fromLookup(const Lookup& lookup);
    /// `sentence` with every whole-word occurrence of `word` blanked.
    static std::string blank(const std::string& sentence, const std::string& word);
    /// Up to `count` cards to practise next: the never-practised first, then
    /// the most-missed, then the longest unseen.
    static std::vector<Card> due(std::vector<Card> cards, size_t count);
};
