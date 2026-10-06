#pragma once

#include "PdfDocument.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

/// How a font turns the bytes a page shows into text, and how far each of
/// its glyphs moves the pen. The font's `ToUnicode` map is trusted first;
/// a simple font without one falls back on its encoding's glyph names.
class PdfFont {
public:
    struct Glyph {
        std::string text;
        /// The advance, in thousandths of the text size.
        double width = 0;
        /// The one-byte code 32, which word spacing widens.
        bool isSpace = false;
    };

    /// A font the page names but does not define: one byte a code, as
    /// WinAnsiEncoding reads it.
    PdfFont();
    PdfFont(const PdfDocument& document, const PdfObject& font);

    std::vector<Glyph> glyphs(const std::string& bytes) const;

private:
    struct CodeRange {
        size_t bytes;
        uint32_t low;
        uint32_t high;
    };

    bool composite_ = false;
    std::vector<CodeRange> codeRanges_;
    std::map<uint32_t, std::string> unicode_;
    std::array<uint32_t, 256> simple_{};
    std::map<uint32_t, double> widths_;
    double defaultWidth_ = 500;
    /// Type 3 glyph widths are in the font's own units.
    double widthScale_ = 1;

    void readToUnicode(const std::string& cmap);
    void readEncoding(const PdfDocument& document, const PdfObject& font);
    void readWidths(const PdfDocument& document, const PdfObject& font);
    size_t codeLength(const std::string& bytes, size_t at) const;
};
