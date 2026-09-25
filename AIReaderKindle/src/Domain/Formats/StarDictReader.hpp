#pragma once

#include "DictionaryFormat.hpp"

/// StarDict: an `.ifo` describing the dictionary, an `.idx` listing every
/// headword with where its article sits, and a `.dict` (often gzipped as
/// `.dict.dz`) holding the articles themselves.
namespace StarDictReader {

/// Fails with a message when the `.idx` or `.dict` is not beside the `.ifo`.
std::optional<DictionaryImportInfo> read(const DictionarySource& source, const DictionaryEntrySink& entry, std::string* error);

}  // namespace StarDictReader

/// Enough of an HTML/XDXF stripper for dictionary articles.
namespace Markup {

std::string stripTags(const std::string& text);

}  // namespace Markup
