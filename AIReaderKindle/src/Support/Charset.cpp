#include "Charset.hpp"

#include "Text.hpp"
#include "TextFile.hpp"

#include <glib.h>

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
// Code points for bytes 0x80–0x9F; from 0xA0 the byte is the code point.
const unsigned short cp1252[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
};

void append(unsigned codePoint, std::string& out) {
    char buffer[6];
    out.append(buffer, g_unichar_to_utf8(codePoint, buffer));
}

std::string singleByte(const std::string& bytes, bool cyrillic) {
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) {
        if (b < 0x80) out += static_cast<char>(b);
        else if (cyrillic) append(cp1251[b - 0x80], out);
        else append(b < 0xA0 ? cp1252[b - 0x80] : b, out);
    }
    return out;
}

bool isAny(const std::string& name, std::initializer_list<const char*> names) {
    for (const char* candidate : names) {
        if (name == candidate) return true;
    }
    return false;
}

/// More letter bytes in 0xC0–0xFF, where windows-1251 keeps А–я, than ASCII letters.
bool looksCyrillic(const std::string& bytes) {
    size_t high = 0, ascii = 0;
    for (unsigned char b : bytes) {
        if (b >= 0xC0) ++high;
        else if ((b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z')) ++ascii;
    }
    return high > ascii;
}

}  // namespace

std::string declared(const std::string& bytes) {
    std::string head = bytes.substr(0, 1024);
    static const std::regex xml(R"(<\?xml[^>]*encoding\s*=\s*["']([^"']+))", std::regex::icase);
    static const std::regex meta(R"(<meta[^>]*charset\s*=\s*["']?([\w-]+))", std::regex::icase);
    std::smatch match;
    if (std::regex_search(head, match, xml) || std::regex_search(head, match, meta)) {
        return Text::lower(Text::trim(match[1].str()));
    }
    return "";
}

std::string decode(const std::string& bytes, const std::string& declared) {
    bool bom = Text::startsWith(bytes, "\xEF\xBB\xBF") || Text::startsWith(bytes, "\xFF\xFE") || Text::startsWith(bytes, "\xFE\xFF");
    // UTF-16 without a mark shows itself by its zero bytes, which UTF-8 also allows.
    if (bom || (bytes.size() > 1 && (bytes[0] == 0 || bytes[1] == 0))) return TextFile::decode(bytes);
    if (isAny(declared, {"windows-1251", "cp1251", "x-cp1251"})) return singleByte(bytes, true);
    if (isAny(declared, {"windows-1252", "cp1252", "iso-8859-1", "latin1", "us-ascii", "ascii"})) return singleByte(bytes, false);
    if (g_utf8_validate(bytes.data(), static_cast<gssize>(bytes.size()), nullptr)) return bytes;
    return singleByte(bytes, looksCyrillic(bytes));
}

}  // namespace Charset
