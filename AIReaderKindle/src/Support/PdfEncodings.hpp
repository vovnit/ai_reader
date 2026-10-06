#pragma once

#include <array>
#include <cstdint>
#include <string>

/// The character sets a PDF's simple fonts and text strings are written in.
namespace PdfEncodings {

/// The code points of a simple font's 256 codes under a base encoding —
/// `WinAnsiEncoding`, `MacRomanEncoding`, or `StandardEncoding` for any
/// other name; 0 where a code has no character.
const std::array<uint32_t, 256>& base(const std::string& name);

/// The code point a glyph name stands for: the standard Latin names,
/// `uniXXXX` and `uXXXX`; 0 when it is not known. A name with a suffix,
/// `a.sc`, is its base name's character.
uint32_t glyph(const std::string& name);

/// A text string — a title, a bookmark — as UTF-8: UTF-16 after its byte
/// order mark, UTF-8 after its own, or else PDFDocEncoding. Trimmed.
std::string text(const std::string& bytes);

/// Appends a character as UTF-8; a ligature, `ﬁ`, as its letters, so the
/// words it is in can be searched.
void append(std::string& text, uint32_t character);

}  // namespace PdfEncodings
