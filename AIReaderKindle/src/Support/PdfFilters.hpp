#pragma once

#include <optional>
#include <string>

/// Undoes the encodings a PDF stream's text may come in. Image encodings
/// are not among them: a page's text never needs its pictures.
namespace PdfFilters {

/// The bytes with one filter undone, by its name or abbreviation; none
/// when the filter is not one of these.
std::optional<std::string> decode(const std::string& filter, const std::string& data);

}  // namespace PdfFilters
