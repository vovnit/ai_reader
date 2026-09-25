#pragma once

#include "Support/Json.hpp"
#include "../Dictionary/DictionaryLookup.hpp"

#include <string>

/// The instructions and tool description that drive a word explanation.
namespace ExplanationPrompt {

extern const char* const system;

std::string question(const std::string& word, const std::string& sentence, const DictionaryLookup& lookup);
/// The dictionary and the book search.
Json tools();

}  // namespace ExplanationPrompt
