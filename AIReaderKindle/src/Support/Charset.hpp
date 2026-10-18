#pragma once

#include <string>

/// The text of a book file in whichever encoding it came in. A byte-order
/// mark settles it; UTF-16 without one shows itself by its zero bytes; then
/// UTF-8 wins whenever most of its multi-byte sequences are valid, whatever
/// the file declares, since declarations are often wrong and a stray bad
/// byte should cost one character, not the book. Otherwise FB2 and XHTML
/// name a single-byte encoding in the XML prolog and HTML in a meta tag, and
/// a file that names none is guessed. Bad input is replaced as a browser's
/// TextDecoder replaces it, and the tables are the browsers' own, so a file
/// reads here as it does in the web app (`Support/Charset.js`) and on iOS
/// (`Support/Charset.swift`).
namespace Charset {

/// The encoding a file names in its first bytes, `<?xml encoding="…"?>` or
/// `<meta charset="…">`, lowercased; empty when none.
std::string declared(const std::string& bytes);

/// `bytes` as UTF-8; `declared` is the encoding the file names, if any.
std::string decode(const std::string& bytes, const std::string& declared = "");

}  // namespace Charset
