#include "Charset.hpp"

#include "Text.hpp"

#include <glib.h>

#include <algorithm>
#include <regex>

namespace Charset {

namespace {

// Code points for bytes 0x80–0xFF.
const unsigned short cp1251[128] = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
    0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x0098, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
    0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
    0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
    0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
    0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
    0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
    0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
};
// Code points for bytes 0x80–0xFF; lib.ru still serves Russian in KOI8-R.
const unsigned short koi8r[128] = {
    0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 0x2524,
    0x252C, 0x2534, 0x253C, 0x2580, 0x2584, 0x2588, 0x258C, 0x2590,
    0x2591, 0x2592, 0x2593, 0x2320, 0x25A0, 0x2219, 0x221A, 0x2248,
    0x2264, 0x2265, 0x00A0, 0x2321, 0x00B0, 0x00B2, 0x00B7, 0x00F7,
    0x2550, 0x2551, 0x2552, 0x0451, 0x2553, 0x2554, 0x2555, 0x2556,
    0x2557, 0x2558, 0x2559, 0x255A, 0x255B, 0x255C, 0x255D, 0x255E,
    0x255F, 0x2560, 0x2561, 0x0401, 0x2562, 0x2563, 0x2564, 0x2565,
    0x2566, 0x2567, 0x2568, 0x2569, 0x256A, 0x256B, 0x256C, 0x00A9,
    0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
    0x0445, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
    0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
    0x044C, 0x044B, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x044A,
    0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
    0x0425, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
    0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
    0x042C, 0x042B, 0x0417, 0x0428, 0x042D, 0x0429, 0x0427, 0x042A,
};
// Code points for bytes 0x80–0x9F; from 0xA0 the byte is the code point.
const unsigned short cp1252[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
};

/// Adds a decoded character as the EPUB can carry it: a byte-order mark
/// doubled at the start dropped, a form feed or vertical tab a line break (a
/// form-feed line is a page break, so it ends a paragraph), and the other
/// controls and the non-characters U+FFFE and U+FFFF, which XML forbids,
/// dropped.
void add(unsigned code, std::string& out) {
    if (code == 0xFEFF && out.empty()) return;
    if (code == 0x0B || code == 0x0C) code = '\n';
    else if (code < 0x09 || (code >= 0x0E && code <= 0x1F) || code == 0x7F || code == 0xFFFE || code == 0xFFFF) return;
    char buffer[6];
    out.append(buffer, g_unichar_to_utf8(code, buffer));
}

/// UTF-8 as a browser's TextDecoder reads it, and how many multi-byte
/// sequences were valid and how many replaced.
struct Utf8 {
    std::string text;
    size_t valid = 0;
    size_t invalid = 0;
};

Utf8 utf8(const std::string& bytes, size_t start) {
    Utf8 result;
    unsigned code = 0, needed = 0, seen = 0, lower = 0x80, upper = 0xBF;
    for (size_t i = start; i < bytes.size(); ++i) {
        unsigned char b = bytes[i];
        if (needed == 0) {
            if (b < 0x80) {
                add(b, result.text);
            } else if (b >= 0xC2 && b <= 0xDF) {
                needed = 1;
                code = b & 0x1F;
            } else if (b >= 0xE0 && b <= 0xEF) {
                // The bounds rule out overlong forms and surrogates.
                if (b == 0xE0) lower = 0xA0;
                if (b == 0xED) upper = 0x9F;
                needed = 2;
                code = b & 0x0F;
            } else if (b >= 0xF0 && b <= 0xF4) {
                if (b == 0xF0) lower = 0x90;
                if (b == 0xF4) upper = 0x8F;
                needed = 3;
                code = b & 0x07;
            } else {
                add(0xFFFD, result.text);
                ++result.invalid;
            }
            continue;
        }
        if (b < lower || b > upper) {
            // The sequence so far is one replacement, and this byte starts afresh.
            add(0xFFFD, result.text);
            ++result.invalid;
            needed = seen = 0;
            lower = 0x80;
            upper = 0xBF;
            --i;
            continue;
        }
        lower = 0x80;
        upper = 0xBF;
        code = (code << 6) | (b & 0x3F);
        if (++seen == needed) {
            add(code, result.text);
            ++result.valid;
            needed = seen = 0;
        }
    }
    if (needed) {
        add(0xFFFD, result.text);
        ++result.invalid;
    }
    return result;
}

/// UTF-16 from `start`, a lone surrogate or an odd last byte replaced as a
/// browser's TextDecoder does.
std::string utf16(const std::string& bytes, size_t start, bool bigEndian) {
    std::string out;
    unsigned high = 0;
    size_t i = start;
    for (; i + 1 < bytes.size(); i += 2) {
        unsigned char a = bytes[i], b = bytes[i + 1];
        unsigned unit = bigEndian ? (a << 8 | b) : (b << 8 | a);
        if (high) {
            if (unit >= 0xDC00 && unit <= 0xDFFF) {
                add(0x10000 + ((high - 0xD800) << 10) + (unit - 0xDC00), out);
                high = 0;
                continue;
            }
            add(0xFFFD, out);
            high = 0;
        }
        if (unit >= 0xD800 && unit <= 0xDBFF) high = unit;
        else add(unit >= 0xDC00 && unit <= 0xDFFF ? 0xFFFD : unit, out);
    }
    // A pair cut short by the end, an odd last byte, or both: one replacement.
    if (high || i < bytes.size()) add(0xFFFD, out);
    return out;
}

unsigned windows1251(unsigned char b) { return cp1251[b - 0x80]; }
unsigned windows1252(unsigned char b) { return b < 0xA0 ? cp1252[b - 0x80] : b; }
unsigned koi8(unsigned char b) { return koi8r[b - 0x80]; }

std::string singleByte(const std::string& bytes, unsigned (*high)(unsigned char)) {
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) add(b < 0x80 ? b : high(b), out);
    return out;
}

/// UTF-16 without a mark: its zero bytes fall at odd offsets (little-endian)
/// or even ones, in at least one pair in sixteen of the first 1024 bytes, and
/// hardly ever at the others, so a stray NUL in a UTF-8 file does not count,
/// nor a run of them padding one out. "le", "be" or empty.
std::string unmarkedUtf16(const std::string& bytes) {
    size_t pairs = std::min<size_t>(bytes.size(), 1024) / 2;
    size_t even = 0, odd = 0;
    for (size_t i = 0; i < pairs * 2; ++i) {
        if (bytes[i] == 0) ++(i % 2 ? odd : even);
    }
    size_t zeros = std::max(even, odd);
    if (zeros == 0 || zeros * 16 < pairs || std::min(even, odd) * 4 >= zeros) return "";
    return odd > even ? "le" : "be";
}

bool isLetter(unsigned char b) {
    return (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z');
}

/// Cyrillic words in windows-1251 are runs of bytes from 0xC0, where А–я
/// are; a Western accent sits inside a Latin word. Markup and base64 have
/// no high bytes, so they do not sway it.
bool looksCyrillic(const std::string& bytes) {
    size_t runs = 0, mixed = 0;
    for (size_t i = 1; i < bytes.size(); ++i) {
        unsigned char a = bytes[i - 1], b = bytes[i];
        if (a >= 0xC0 && b >= 0xC0) ++runs;
        else if ((a >= 0xC0 && isLetter(b)) || (b >= 0xC0 && isLetter(a))) ++mixed;
    }
    return runs > mixed;
}

/// Most letters are lower case, and KOI8-R puts а–я at 0xC0–0xDF where
/// windows-1251 has А–Я, its own а–я being at 0xE0–0xFF.
bool looksKoi8(const std::string& bytes) {
    size_t lower = 0, upper = 0;
    for (unsigned char b : bytes) {
        if (b >= 0xE0) ++upper;
        else if (b >= 0xC0) ++lower;
    }
    return lower > upper;
}

bool isAny(const std::string& name, std::initializer_list<const char*> names) {
    for (const char* candidate : names) {
        if (name == candidate) return true;
    }
    return false;
}

}  // namespace

std::string declared(const std::string& bytes) {
    // ASCII only, lowercased, so every app reads the same name.
    std::string head = bytes.substr(0, 1024);
    for (char& c : head) {
        unsigned char b = c;
        if (b >= 0x80) c = '?';
        else if (b >= 'A' && b <= 'Z') c = static_cast<char>(b + 0x20);
    }
    static const std::regex xml(R"(<\?xml[^>]*encoding[ \t\r\n]*=[ \t\r\n]*["']([^"']+))");
    static const std::regex meta(R"(<meta[^>]*charset[ \t\r\n]*=[ \t\r\n]*["']?([a-z0-9_-]+))");
    std::smatch match;
    if (std::regex_search(head, match, xml) || std::regex_search(head, match, meta)) return Text::trim(match[1].str());
    return "";
}

std::string decode(const std::string& bytes, const std::string& declared) {
    if (Text::startsWith(bytes, "\xEF\xBB\xBF")) return utf8(bytes, 3).text;
    if (Text::startsWith(bytes, "\xFF\xFE")) return utf16(bytes, 2, false);
    if (Text::startsWith(bytes, "\xFE\xFF")) return utf16(bytes, 2, true);
    std::string unmarked = unmarkedUtf16(bytes);
    if (!unmarked.empty()) return utf16(bytes, 0, unmarked == "be");
    Utf8 unicode = utf8(bytes, 0);
    if (unicode.valid > unicode.invalid) return unicode.text;
    if (isAny(declared, {"windows-1251", "cp1251", "x-cp1251"})) return singleByte(bytes, windows1251);
    if (isAny(declared, {"koi8-r", "koi8-u"})) return singleByte(bytes, koi8);
    if (isAny(declared, {"windows-1252", "cp1252", "iso-8859-1", "latin1", "us-ascii", "ascii"})) return singleByte(bytes, windows1252);
    if (!looksCyrillic(bytes)) return singleByte(bytes, windows1252);
    return singleByte(bytes, looksKoi8(bytes) ? koi8 : windows1251);
}

}  // namespace Charset
