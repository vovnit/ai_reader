#include "PdfFont.hpp"

#include "PdfEncodings.hpp"

#include <algorithm>

namespace {

uint32_t value(const std::string& bytes) {
    uint32_t code = 0;
    for (unsigned char byte : bytes) code = (code << 8) | byte;
    return code;
}

std::string fromUtf16(const std::string& bytes) {
    std::string text;
    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
        uint32_t unit = (static_cast<unsigned char>(bytes[i]) << 8) | static_cast<unsigned char>(bytes[i + 1]);
        if (unit >= 0xD800 && unit < 0xDC00 && i + 3 < bytes.size()) {
            uint32_t low = (static_cast<unsigned char>(bytes[i + 2]) << 8) | static_cast<unsigned char>(bytes[i + 3]);
            unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
            i += 2;
        }
        PdfEncodings::append(text, unit);
    }
    // A single byte, against the rule, is taken as the character itself.
    if (bytes.size() == 1) PdfEncodings::append(text, static_cast<unsigned char>(bytes[0]));
    return text;
}

/// `base`, a UTF-16 string, with its last unit moved on by `step`: how a
/// `bfrange` numbers the characters of its codes.
std::string stepped(std::string base, uint32_t step) {
    if (base.size() < 2) return base;
    uint32_t last = value(base.substr(base.size() - 2)) + step;
    base[base.size() - 2] = static_cast<char>((last >> 8) & 0xFF);
    base[base.size() - 1] = static_cast<char>(last & 0xFF);
    return base;
}

}  // namespace

PdfFont::PdfFont() : simple_(PdfEncodings::base("WinAnsiEncoding")) {}

PdfFont::PdfFont(const PdfDocument& document, const PdfObject& reference) {
    const PdfObject& font = document.resolve(reference);
    composite_ = document.get(font, "Subtype").isName("Type0");
    if (composite_) {
        // The encoding's code ranges say how many bytes each code takes;
        // a named one, Identity-H, takes two.
        const PdfObject& encoding = document.get(font, "Encoding");
        if (encoding.is(PdfObject::Type::Stream)) readToUnicode(document.contents(encoding));
        std::vector<CodeRange> fromEncoding = codeRanges_;
        const PdfObject& toUnicode = document.get(font, "ToUnicode");
        if (toUnicode.is(PdfObject::Type::Stream)) readToUnicode(document.contents(toUnicode));
        codeRanges_ = fromEncoding.empty() ? std::vector<CodeRange>{{2, 0, 0xFFFF}} : fromEncoding;
    } else {
        readEncoding(document, font);
        const PdfObject& toUnicode = document.get(font, "ToUnicode");
        if (toUnicode.is(PdfObject::Type::Stream)) readToUnicode(document.contents(toUnicode));
    }
    readWidths(document, font);
}

void PdfFont::readToUnicode(const std::string& cmap) {
    PdfLexer lexer(cmap);
    std::vector<PdfObject> operands;
    while (!lexer.atEnd()) {
        PdfObject token = lexer.next();
        if (!token.is(PdfObject::Type::Operator)) {
            operands.push_back(std::move(token));
            continue;
        }
        if (token.text == "endcodespacerange") {
            for (size_t i = 0; i + 1 < operands.size(); i += 2) {
                const std::string& low = operands[i].text;
                if (!low.empty() && low.size() <= 4) codeRanges_.push_back({low.size(), value(low), value(operands[i + 1].text)});
            }
            std::sort(codeRanges_.begin(), codeRanges_.end(), [](const CodeRange& a, const CodeRange& b) { return a.bytes < b.bytes; });
        } else if (token.text == "endbfchar") {
            for (size_t i = 0; i + 1 < operands.size(); i += 2) {
                const PdfObject& target = operands[i + 1];
                unicode_[value(operands[i].text)] = target.is(PdfObject::Type::Name)
                    ? std::string() : fromUtf16(target.text);
            }
        } else if (token.text == "endbfrange") {
            for (size_t i = 0; i + 2 < operands.size(); i += 3) {
                uint32_t low = value(operands[i].text);
                uint32_t high = value(operands[i + 1].text);
                const PdfObject& target = operands[i + 2];
                if (high < low || high - low > 0xFFFF) continue;
                for (uint32_t code = low; code <= high; ++code) {
                    if (target.is(PdfObject::Type::Array)) {
                        if (code - low < target.items.size()) unicode_[code] = fromUtf16(target.items[code - low].text);
                    } else {
                        unicode_[code] = fromUtf16(stepped(target.text, code - low));
                    }
                }
            }
        }
        operands.clear();
    }
}

void PdfFont::readEncoding(const PdfDocument& document, const PdfObject& font) {
    const PdfObject& encoding = document.get(font, "Encoding");
    std::string base = encoding.is(PdfObject::Type::Name) ? encoding.text : document.get(encoding, "BaseEncoding").text;
    if (base.empty() && document.get(font, "Subtype").isName("TrueType")) base = "WinAnsiEncoding";
    simple_ = PdfEncodings::base(base);
    int code = 0;
    for (const auto& item : document.get(encoding, "Differences").items) {
        const PdfObject& difference = document.resolve(item);
        if (difference.is(PdfObject::Type::Number)) {
            code = difference.integer();
        } else if (difference.is(PdfObject::Type::Name)) {
            if (code >= 0 && code < 256) simple_[code] = PdfEncodings::glyph(difference.text);
            ++code;
        }
    }
}

void PdfFont::readWidths(const PdfDocument& document, const PdfObject& font) {
    if (composite_) {
        const PdfObject& descendants = document.get(font, "DescendantFonts");
        const PdfObject& cid = descendants.items.empty() ? descendants : document.resolve(descendants.items[0]);
        const PdfObject& fallback = document.get(cid, "DW");
        defaultWidth_ = fallback.is(PdfObject::Type::Number) ? fallback.number : 1000;
        const std::vector<PdfObject>& list = document.get(cid, "W").items;
        for (size_t i = 0; i + 1 < list.size();) {
            uint32_t first = static_cast<uint32_t>(document.resolve(list[i]).integer());
            const PdfObject& next = document.resolve(list[i + 1]);
            if (next.is(PdfObject::Type::Array)) {
                for (size_t k = 0; k < next.items.size(); ++k) widths_[first + k] = document.resolve(next.items[k]).number;
                i += 2;
            } else if (i + 2 < list.size()) {
                uint32_t last = static_cast<uint32_t>(next.integer());
                double width = document.resolve(list[i + 2]).number;
                for (uint32_t code = first; code <= last && code - first <= 0xFFFF; ++code) widths_[code] = width;
                i += 3;
            } else {
                break;
            }
        }
        return;
    }
    int first = document.get(font, "FirstChar").integer();
    const std::vector<PdfObject>& list = document.get(font, "Widths").items;
    for (size_t k = 0; k < list.size(); ++k) widths_[static_cast<uint32_t>(first + static_cast<int>(k))] = document.resolve(list[k]).number;
    const PdfObject& missing = document.get(document.get(font, "FontDescriptor"), "MissingWidth");
    if (missing.is(PdfObject::Type::Number) && missing.number > 0) defaultWidth_ = missing.number;
    // The standard fonts may come without widths; Courier's are all 600.
    else if (list.empty() && document.get(font, "BaseFont").text.find("Courier") != std::string::npos) defaultWidth_ = 600;
    const PdfObject& matrix = document.get(font, "FontMatrix");
    if (document.get(font, "Subtype").isName("Type3") && !matrix.items.empty()) {
        widthScale_ = document.resolve(matrix.items[0]).number * 1000;
    }
}

size_t PdfFont::codeLength(const std::string& bytes, size_t at) const {
    for (const auto& range : codeRanges_) {
        if (at + range.bytes > bytes.size()) continue;
        uint32_t code = value(bytes.substr(at, range.bytes));
        if (code >= range.low && code <= range.high) return range.bytes;
    }
    return std::min<size_t>(2, bytes.size() - at);
}

std::vector<PdfFont::Glyph> PdfFont::glyphs(const std::string& bytes) const {
    std::vector<Glyph> glyphs;
    for (size_t at = 0; at < bytes.size();) {
        size_t length = composite_ ? codeLength(bytes, at) : 1;
        uint32_t code = value(bytes.substr(at, length));
        Glyph glyph;
        auto mapped = unicode_.find(code);
        if (mapped != unicode_.end()) glyph.text = mapped->second;
        else if (!composite_) PdfEncodings::append(glyph.text, simple_[code & 0xFF]);
        auto width = widths_.find(code);
        glyph.width = (width != widths_.end() ? width->second : defaultWidth_) * widthScale_;
        glyph.isSpace = length == 1 && code == 32;
        glyphs.push_back(std::move(glyph));
        at += length;
    }
    return glyphs;
}
