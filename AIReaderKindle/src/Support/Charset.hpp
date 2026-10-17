#pragma once

#include <string>

/// The text of a book file in whichever encoding it came in. FB2 and XHTML
/// declare theirs in the XML prolog and HTML in a meta tag; a .txt says
/// nothing, so a file that is not UTF-8 is read as windows-1251 when more of
/// its letters are Cyrillic than Latin, else as windows-1252. The tables are
/// the browsers' own, so a file reads here as it does in the web app
/// (`Support/Charset.js`) and on iOS (`Support/Charset.swift`).
namespace Charset {

/// The encoding a file names in its first bytes, `<?xml encoding="…"?>` or
/// `<meta charset="…">`, lowercased; empty when none.
std::string declared(const std::string& bytes);

/// `bytes` as UTF-8; `declared` is the encoding the file names, if any.
std::string decode(const std::string& bytes, const std::string& declared = "");

}  // namespace Charset
