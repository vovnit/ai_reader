#pragma once

#include <string>

/// Makes an EPUB of an FB2 (bare or zipped), a plain-text file or an HTML
/// page, so it is read, searched and synced like any other book. The file's
/// name says which it is. The web app's `BookImporter.js` and the iOS
/// app's `BookImporter.swift` take files in by the same rules, so a file
/// makes the same book, with the same chapters, on every device. A PDF is
/// `PdfImporter`'s.
namespace BookImporter {

/// Whether the file's name, in any case, ends `.fb2`, `.fb2.zip`, `.fbz`,
/// `.zip` (any ZIP is looked into for an FB2, the way FB2 books are shared,
/// or else an EPUB), `.txt`, `.html`, `.htm` or `.xhtml`.
bool reads(const std::string& path);

/// The EPUB for a file's bytes; the file's name, at the end of `path`, says
/// its format and names a book that does not name itself. A ZIP holding an
/// EPUB rather than an FB2 is that EPUB, as it is. Throws
/// `std::runtime_error` with a reason fit to show.
std::string epub(const std::string& file, const std::string& path);

/// Writes the EPUB of the file at `path` into `folder`, named after it
/// ("x.fb2.zip" makes "x.epub"), and returns its path; the EPUB made of the
/// same file before is found again (`EpubWriter::destination`). A ZIP that
/// is an EPUB is copied there as one. Empty, with `error` set, when it
/// cannot be made.
std::string convert(const std::string& path, const std::string& folder, std::string* error);

}  // namespace BookImporter
