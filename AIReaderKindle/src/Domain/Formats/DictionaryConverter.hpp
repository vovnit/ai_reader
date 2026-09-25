#pragma once

#include "DictionaryFormat.hpp"

/// Reads a dictionary in any of the formats readers hand around, handing
/// each article to `entry`. Fails with a message fit to show.
namespace DictionaryConverter {

std::optional<DictionaryImportInfo> read(const DictionarySource& source, const DictionaryEntrySink& entry, std::string* error);

}  // namespace DictionaryConverter
