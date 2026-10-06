#pragma once

#include <string>

/// Makes an EPUB of a PDF, so it is read, searched and synced like any
/// other book: the PDF's text laid out again as chapters and paragraphs.
/// A scanned PDF has no text to take; ScanTool is for those.
namespace PdfImporter {

/// The EPUB for a PDF's bytes; `fallbackTitle` names a PDF that does not
/// name itself. Throws `std::runtime_error` with a reason fit to show.
std::string epub(const std::string& pdf, const std::string& fallbackTitle);

/// Writes the EPUB of the PDF at `path` into `folder`, named after it, and
/// returns its path. An EPUB of that name already there is taken to be
/// this one, made before. Empty, with `error` set, when it cannot be made.
std::string convert(const std::string& path, const std::string& folder, std::string* error);

}  // namespace PdfImporter
