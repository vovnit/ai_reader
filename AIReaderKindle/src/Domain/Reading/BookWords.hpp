#pragma once

#include <string>
#include <vector>

/// One distinct word form of a book: what its glossary is written from.
struct BookWord {
    /// As the dictionaries look it up.
    std::string form;
    /// As the book first writes it, capital and all, which tells a name apart.
    std::string spelling;
    /// The first places it appears, a few words either side.
    std::vector<std::string> examples;
};

/// Every distinct word form in a book, in reading order, with the first
/// places it appears. Words are the ones a tap finds (WordContext), normalized
/// as the dictionaries look them up. Only the first places, so a definition
/// of a name cannot give away what happens later.
namespace BookWords {

/// `chapters` are the chapters' texts, a paragraph a line.
std::vector<BookWord> collect(const std::vector<std::string>& chapters, const std::string& language);

}  // namespace BookWords
