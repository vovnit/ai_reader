#pragma once

#include "Support/Json.hpp"

#include <optional>
#include <string>

/// What the model produces for a looked-up word.
struct WordExplanation {
    /// The dictionary form of the word.
    std::string lemma;
    /// How the form found in the text relates to the lemma.
    std::string formNote;
    /// A short, plain explanation of what the word means here.
    std::string meaning;
    /// True when no dictionary entry supported the answer.
    bool guessed = false;
    /// The model's own confidence, from 0 to 1.
    double confidence = 0;

    Json toJson() const;
    /// Reads the JSON object out of a reply, tolerating text around it.
    static std::optional<WordExplanation> decode(const std::string& content);
};

/// Where a lookup came from: the word, the sentence around it, and the book it
/// was read in.
struct LookupContext {
    std::string word;
    std::string sentence;
    std::string language;
    long long bookId = 0;
};

/// A past lookup. Doubles as the cache: repeating a lookup of the same word in
/// the same sentence reuses the stored answer.
struct Lookup {
    long long id = 0;
    std::string word;
    std::string sentence;
    std::string lemma;
    std::string formNote;
    std::string meaning;
    std::string language;
    long long bookId = 0;
    bool guessed = false;
    double confidence = 0;
    std::string lookedUpAt;

    WordExplanation explanation() const { return {lemma, formNote, meaning, guessed, confidence}; }
};
