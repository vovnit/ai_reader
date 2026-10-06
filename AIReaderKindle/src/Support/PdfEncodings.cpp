#include "PdfEncodings.hpp"

#include <glib.h>

#include <cstdlib>
#include <sstream>
#include <unordered_map>

namespace PdfEncodings {

namespace {

// Windows-1252's 0x80–0x9F, which WinAnsiEncoding is.
const uint32_t winAnsiHigh[32] = {
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178,
};

const uint32_t macRomanHigh[128] = {
    0xC4, 0xC5, 0xC7, 0xC9, 0xD1, 0xD6, 0xDC, 0xE1, 0xE0, 0xE2, 0xE4, 0xE3, 0xE5, 0xE7, 0xE9, 0xE8,
    0xEA, 0xEB, 0xED, 0xEC, 0xEE, 0xEF, 0xF1, 0xF3, 0xF2, 0xF4, 0xF6, 0xF5, 0xFA, 0xF9, 0xFB, 0xFC,
    0x2020, 0xB0, 0xA2, 0xA3, 0xA7, 0x2022, 0xB6, 0xDF, 0xAE, 0xA9, 0x2122, 0xB4, 0xA8, 0x2260, 0xC6, 0xD8,
    0x221E, 0xB1, 0x2264, 0x2265, 0xA5, 0xB5, 0x2202, 0x2211, 0x220F, 0x3C0, 0x222B, 0xAA, 0xBA, 0x3A9, 0xE6, 0xF8,
    0xBF, 0xA1, 0xAC, 0x221A, 0x192, 0x2248, 0x2206, 0xAB, 0xBB, 0x2026, 0xA0, 0xC0, 0xC3, 0xD5, 0x152, 0x153,
    0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0xF7, 0x25CA, 0xFF, 0x178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,
    0x2021, 0xB7, 0x201A, 0x201E, 0x2030, 0xC2, 0xCA, 0xC1, 0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0xD3, 0xD4,
    0xF8FF, 0xD2, 0xDA, 0xDB, 0xD9, 0x131, 0x2C6, 0x2DC, 0xAF, 0x2D8, 0x2D9, 0x2DA, 0xB8, 0x2DD, 0x2DB, 0x2C7,
};

// StandardEncoding's upper half, from 0xA0, the default of a Type 1 font.
const uint32_t standardHigh[96] = {
    0, 0xA1, 0xA2, 0xA3, 0x2044, 0xA5, 0x192, 0xA7, 0xA4, 0x27, 0x201C, 0xAB, 0x2039, 0x203A, 0xFB01, 0xFB02,
    0, 0x2013, 0x2020, 0x2021, 0xB7, 0, 0xB6, 0x2022, 0x201A, 0x201E, 0x201D, 0xBB, 0x2026, 0x2030, 0, 0xBF,
    0, 0x60, 0xB4, 0x2C6, 0x2DC, 0xAF, 0x2D8, 0x2D9, 0xA8, 0, 0x2DA, 0xB8, 0, 0x2DD, 0x2DB, 0x2C7,
    0x2014, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0xC6, 0, 0xAA, 0, 0, 0, 0, 0x141, 0xD8, 0x152, 0xBA, 0, 0, 0, 0,
    0, 0xE6, 0, 0, 0, 0x131, 0, 0, 0x142, 0xF8, 0x153, 0xDF, 0, 0, 0, 0,
};

// PDFDocEncoding's 0x80–0xA0; the rest is Latin-1.
const uint32_t pdfDocHigh[33] = {
    0x2022, 0x2020, 0x2021, 0x2026, 0x2014, 0x2013, 0x192, 0x2044, 0x2039, 0x203A, 0x2212, 0x2030, 0x201E, 0x201C, 0x201D, 0x2018,
    0x2019, 0x201A, 0x2122, 0xFB01, 0xFB02, 0x141, 0x152, 0x160, 0x178, 0x17D, 0x131, 0x142, 0x153, 0x161, 0x17E, 0, 0x20AC,
};

// Glyph names and their characters, in hex: the names the standard Latin
// encodings use, and those of Latin Extended-A. Single letters are
// themselves and are not listed.
const char* const glyphNames =
    "space 20 exclam 21 quotedbl 22 numbersign 23 dollar 24 percent 25 ampersand 26 quotesingle 27 quoteright 2019 "
    "parenleft 28 parenright 29 asterisk 2A plus 2B comma 2C hyphen 2D period 2E slash 2F zero 30 one 31 two 32 "
    "three 33 four 34 five 35 six 36 seven 37 eight 38 nine 39 colon 3A semicolon 3B less 3C equal 3D greater 3E "
    "question 3F at 40 bracketleft 5B backslash 5C bracketright 5D asciicircum 5E underscore 5F grave 60 "
    "quoteleft 2018 braceleft 7B bar 7C braceright 7D asciitilde 7E nbspace A0 nonbreakingspace A0 exclamdown A1 "
    "cent A2 sterling A3 currency A4 yen A5 brokenbar A6 section A7 dieresis A8 copyright A9 ordfeminine AA "
    "guillemotleft AB logicalnot AC sfthyphen AD registered AE macron AF degree B0 plusminus B1 twosuperior B2 "
    "threesuperior B3 acute B4 mu B5 paragraph B6 periodcentered B7 cedilla B8 onesuperior B9 ordmasculine BA "
    "guillemotright BB onequarter BC onehalf BD threequarters BE questiondown BF Agrave C0 Aacute C1 "
    "Acircumflex C2 Atilde C3 Adieresis C4 Aring C5 AE C6 Ccedilla C7 Egrave C8 Eacute C9 Ecircumflex CA "
    "Edieresis CB Igrave CC Iacute CD Icircumflex CE Idieresis CF Eth D0 Ntilde D1 Ograve D2 Oacute D3 "
    "Ocircumflex D4 Otilde D5 Odieresis D6 multiply D7 Oslash D8 Ugrave D9 Uacute DA Ucircumflex DB Udieresis DC "
    "Yacute DD Thorn DE germandbls DF agrave E0 aacute E1 acircumflex E2 atilde E3 adieresis E4 aring E5 ae E6 "
    "ccedilla E7 egrave E8 eacute E9 ecircumflex EA edieresis EB igrave EC iacute ED icircumflex EE idieresis EF "
    "eth F0 ntilde F1 ograve F2 oacute F3 ocircumflex F4 otilde F5 odieresis F6 divide F7 oslash F8 ugrave F9 "
    "uacute FA ucircumflex FB udieresis FC yacute FD thorn FE ydieresis FF Amacron 100 amacron 101 Abreve 102 "
    "abreve 103 Aogonek 104 aogonek 105 Cacute 106 cacute 107 Ccaron 10C ccaron 10D Dcaron 10E dcaron 10F "
    "Dcroat 110 dcroat 111 Emacron 112 emacron 113 Edotaccent 116 edotaccent 117 Eogonek 118 eogonek 119 "
    "Ecaron 11A ecaron 11B Gbreve 11E gbreve 11F Idotaccent 130 dotlessi 131 Lacute 139 lacute 13A Lcaron 13D "
    "lcaron 13E Lslash 141 lslash 142 Nacute 143 nacute 144 Ncaron 147 ncaron 148 Ohungarumlaut 150 "
    "ohungarumlaut 151 OE 152 oe 153 Racute 154 racute 155 Rcaron 158 rcaron 159 Sacute 15A sacute 15B "
    "Scedilla 15E scedilla 15F Scaron 160 scaron 161 Tcaron 164 tcaron 165 Uring 16E uring 16F Uhungarumlaut 170 "
    "uhungarumlaut 171 Ydieresis 178 Zacute 179 zacute 17A Zdotaccent 17B zdotaccent 17C Zcaron 17D zcaron 17E "
    "florin 192 circumflex 2C6 caron 2C7 breve 2D8 dotaccent 2D9 ring 2DA ogonek 2DB tilde 2DC hungarumlaut 2DD "
    "pi 3C0 endash 2013 emdash 2014 quotesinglbase 201A quotedblleft 201C quotedblright 201D quotedblbase 201E "
    "dagger 2020 daggerdbl 2021 bullet 2022 ellipsis 2026 perthousand 2030 guilsinglleft 2039 guilsinglright 203A "
    "fraction 2044 Euro 20AC trademark 2122 Omega 2126 partialdiff 2202 Delta 2206 product 220F summation 2211 "
    "minus 2212 radical 221A infinity 221E integral 222B approxequal 2248 notequal 2260 lessequal 2264 "
    "greaterequal 2265 lozenge 25CA ff FB00 fi FB01 fl FB02 ffi FB03 ffl FB04";

std::array<uint32_t, 256> table(const uint32_t* high, size_t from, size_t count) {
    std::array<uint32_t, 256> codes{};
    for (uint32_t code = 0x20; code < 0x7F; ++code) codes[code] = code;
    for (size_t i = 0; i < count; ++i) codes[from + i] = high[i];
    return codes;
}

uint32_t hex(const std::string& digits) {
    for (char c : digits) {
        if (!g_ascii_isxdigit(c)) return 0;
    }
    return static_cast<uint32_t>(std::strtoul(digits.c_str(), nullptr, 16));
}

}  // namespace

const std::array<uint32_t, 256>& base(const std::string& name) {
    static const std::array<uint32_t, 256> winAnsi = [] {
        auto codes = table(winAnsiHigh, 0x80, 32);
        for (uint32_t code = 0xA0; code <= 0xFF; ++code) codes[code] = code;
        return codes;
    }();
    static const std::array<uint32_t, 256> macRoman = table(macRomanHigh, 0x80, 128);
    static const std::array<uint32_t, 256> standard = [] {
        auto codes = table(standardHigh, 0xA0, 96);
        codes[0x27] = 0x2019;
        codes[0x60] = 0x2018;
        return codes;
    }();
    if (name == "WinAnsiEncoding") return winAnsi;
    if (name == "MacRomanEncoding") return macRoman;
    return standard;
}

uint32_t glyph(const std::string& full) {
    static const std::unordered_map<std::string, uint32_t> names = [] {
        std::unordered_map<std::string, uint32_t> map;
        std::istringstream list(glyphNames);
        std::string name;
        std::string value;
        while (list >> name >> value) map[name] = hex(value);
        return map;
    }();
    std::string name = full.substr(0, full.find('.'));
    if (name.size() == 1 && g_ascii_isalpha(name[0])) return static_cast<unsigned char>(name[0]);
    auto found = names.find(name);
    if (found != names.end()) return found->second;
    if (name.size() == 7 && name.compare(0, 3, "uni") == 0) return hex(name.substr(3));
    if (name.size() >= 5 && name.size() <= 7 && name[0] == 'u') return hex(name.substr(1));
    return 0;
}

void append(std::string& text, uint32_t character) {
    static const char* const ligatures[] = {"ff", "fi", "fl", "ffi", "ffl", "ft", "st"};
    if (character >= 0xFB00 && character <= 0xFB06) {
        text += ligatures[character - 0xFB00];
        return;
    }
    if (character == 0 || character > 0x10FFFF || (character >= 0xD800 && character < 0xE000)) return;
    char bytes[6];
    text.append(bytes, g_unichar_to_utf8(character, bytes));
}

std::string text(const std::string& bytes) {
    std::string out;
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE && static_cast<unsigned char>(bytes[1]) == 0xFF) {
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            uint32_t unit = (static_cast<unsigned char>(bytes[i]) << 8) | static_cast<unsigned char>(bytes[i + 1]);
            if (unit >= 0xD800 && unit < 0xDC00 && i + 3 < bytes.size()) {
                uint32_t low = (static_cast<unsigned char>(bytes[i + 2]) << 8) | static_cast<unsigned char>(bytes[i + 3]);
                unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                i += 2;
            }
            append(out, unit);
        }
    } else if (bytes.compare(0, 3, "\xEF\xBB\xBF") == 0) {
        out = bytes.substr(3);
    } else {
        for (unsigned char byte : bytes) {
            if (byte >= 0x80 && byte <= 0xA0) append(out, pdfDocHigh[byte - 0x80]);
            else if (byte >= 0x20 || byte == '\t' || byte == '\n' || byte == '\r') append(out, byte);
        }
    }
    // Titles are often padded, or end in a stray terminator.
    size_t start = out.find_first_not_of(" \t\r\n");
    size_t end = out.find_last_not_of(" \t\r\n");
    return start == std::string::npos ? "" : out.substr(start, end - start + 1);
}

}  // namespace PdfEncodings
