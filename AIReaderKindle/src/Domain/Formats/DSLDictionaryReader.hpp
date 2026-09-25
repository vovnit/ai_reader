#pragma once

#include "DictionaryFormat.hpp"

/// ABBYY Lingvo's DSL. Headwords start at column zero; the lines indented
/// under them are the article, marked up with `[tags]` this reader strips.
namespace DSLDictionaryReader {

std::optional<DictionaryImportInfo> read(const DictionarySource& source, const DictionaryEntrySink& entry);

/// Removes DSL markup, keeping the words inside it.
std::string strip(const std::string& line);

}  // namespace DSLDictionaryReader
