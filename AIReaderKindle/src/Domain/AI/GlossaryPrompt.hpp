#pragma once

#include "../Reading/BookWords.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

/// What the model is asked to write a book's glossary — a batch of the book's
/// word forms, each with the first places it appears — and how its answer
/// becomes definitions. The wording is the other apps' and DictionaryTool's
/// `glossary_prompt.py`, word for word.
namespace GlossaryPrompt {

/// Forms asked about in one request.
const size_t batchSize = 50;
/// Room for a batch's definitions; a lookup's limit would cut the answer short.
const int maxTokens = 4000;
/// What a form costs, question and answer together, as measured on a short book.
const long long tokensPerWord = 120;

/// `language` is the one the definitions are written in.
std::string system(const std::string& language);
std::string question(const std::vector<BookWord>& words);
/// Form → definition for each word the answer covers, or nothing when the
/// answer cannot be read. A word the model skipped is simply absent.
std::optional<std::map<std::string, std::string>> definitions(const std::string& content, const std::vector<BookWord>& words);

}  // namespace GlossaryPrompt
