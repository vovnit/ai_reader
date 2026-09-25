#pragma once

#include "DictionaryFormat.hpp"

/// Word lists: one headword and its definition per line, separated by tabs or
/// by commas. The plainest thing a reader is likely to have made themselves.
namespace DelimitedDictionaryReader {

std::optional<DictionaryImportInfo> read(const DictionarySource& source, const DictionaryEntrySink& entry);

/// The same, from text already in memory.
void readText(const std::string& contents, const DictionaryEntrySink& entry);

}  // namespace DelimitedDictionaryReader
