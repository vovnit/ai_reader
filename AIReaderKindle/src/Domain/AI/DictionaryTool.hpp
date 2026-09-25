#pragma once

#include "Support/Json.hpp"

#include <string>

/// The tool that lets the model open the dictionary: `lookup_dictionary`
/// finds a word's articles in the offline packs.
namespace DictionaryTool {

extern const char* const toolName;

Json tool();
std::string word(const std::string& arguments);

}  // namespace DictionaryTool
