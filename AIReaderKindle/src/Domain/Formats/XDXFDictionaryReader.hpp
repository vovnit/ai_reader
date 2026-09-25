#pragma once

#include "DictionaryFormat.hpp"

/// XDXF, the open XML dictionary format. Articles are `<ar>` elements holding
/// one or more `<k>` headwords and the definition text around them.
namespace XDXFDictionaryReader {

std::optional<DictionaryImportInfo> read(const DictionarySource& source, const DictionaryEntrySink& entry);

}  // namespace XDXFDictionaryReader
