#pragma once

#include "../../Domain/AI/WordExplanation.hpp"
#include "../../Services/Env.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

/// Every word looked up so far, newest first — the vocabulary the reader has
/// actually met, rather than a list someone else chose.
class WordsFeature {
public:
    /// `bookId` narrows the list to one book; 0 shows every word met so far.
    WordsFeature(Env& env, long long bookId);

    const std::vector<Lookup>& lookups() const { return lookups_; }
    void reload();
    void remove(const Lookup& lookup);
    /// Writes the listed words as cards for Anki; the path written, or
    /// nothing when the file could not be.
    std::optional<std::string> exportToAnki();

    std::function<void()> onChange;

private:
    Env& env_;
    long long bookId_;
    std::vector<Lookup> lookups_;
};
