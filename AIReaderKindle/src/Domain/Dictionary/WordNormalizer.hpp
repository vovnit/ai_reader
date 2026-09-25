#pragma once

#include <string>

/// Puts a word into the shape the dictionary's `normalized_form` column uses.
namespace WordNormalizer {

std::string normalize(const std::string& word);

}  // namespace WordNormalizer
