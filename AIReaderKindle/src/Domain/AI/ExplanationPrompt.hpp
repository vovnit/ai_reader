#pragma once

#include "Support/Json.hpp"
#include "../Dictionary/DictionaryLookup.hpp"

#include <string>

/// The instructions and tool description that drive a word explanation.
namespace ExplanationPrompt {

/// `language` is the one the explanation is written in.
std::string system(const std::string& language);

std::string question(const std::string& word, const std::string& sentence, const DictionaryLookup& lookup);
/// The dictionary and the book search.
Json tools();

}  // namespace ExplanationPrompt
