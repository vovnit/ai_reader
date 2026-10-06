// The character sets a PDF's simple fonts and text strings are written in.

// Windows-1252's 0x80–0x9f, which WinAnsiEncoding is.
const winAnsiHigh = [
  0x20ac, 0, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017d, 0,
  0, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0, 0x017e, 0x0178,
];

const macRomanHigh = [
  0xc4, 0xc5, 0xc7, 0xc9, 0xd1, 0xd6, 0xdc, 0xe1, 0xe0, 0xe2, 0xe4, 0xe3, 0xe5, 0xe7, 0xe9, 0xe8,
  0xea, 0xeb, 0xed, 0xec, 0xee, 0xef, 0xf1, 0xf3, 0xf2, 0xf4, 0xf6, 0xf5, 0xfa, 0xf9, 0xfb, 0xfc,
  0x2020, 0xb0, 0xa2, 0xa3, 0xa7, 0x2022, 0xb6, 0xdf, 0xae, 0xa9, 0x2122, 0xb4, 0xa8, 0x2260, 0xc6, 0xd8,
  0x221e, 0xb1, 0x2264, 0x2265, 0xa5, 0xb5, 0x2202, 0x2211, 0x220f, 0x3c0, 0x222b, 0xaa, 0xba, 0x3a9, 0xe6, 0xf8,
  0xbf, 0xa1, 0xac, 0x221a, 0x192, 0x2248, 0x2206, 0xab, 0xbb, 0x2026, 0xa0, 0xc0, 0xc3, 0xd5, 0x152, 0x153,
  0x2013, 0x2014, 0x201c, 0x201d, 0x2018, 0x2019, 0xf7, 0x25ca, 0xff, 0x178, 0x2044, 0x20ac, 0x2039, 0x203a, 0xfb01, 0xfb02,
  0x2021, 0xb7, 0x201a, 0x201e, 0x2030, 0xc2, 0xca, 0xc1, 0xcb, 0xc8, 0xcd, 0xce, 0xcf, 0xcc, 0xd3, 0xd4,
  0xf8ff, 0xd2, 0xda, 0xdb, 0xd9, 0x131, 0x2c6, 0x2dc, 0xaf, 0x2d8, 0x2d9, 0x2da, 0xb8, 0x2dd, 0x2db, 0x2c7,
];

// StandardEncoding's upper half, from 0xa0, the default of a Type 1 font.
const standardHigh = [
  0, 0xa1, 0xa2, 0xa3, 0x2044, 0xa5, 0x192, 0xa7, 0xa4, 0x27, 0x201c, 0xab, 0x2039, 0x203a, 0xfb01, 0xfb02,
  0, 0x2013, 0x2020, 0x2021, 0xb7, 0, 0xb6, 0x2022, 0x201a, 0x201e, 0x201d, 0xbb, 0x2026, 0x2030, 0, 0xbf,
  0, 0x60, 0xb4, 0x2c6, 0x2dc, 0xaf, 0x2d8, 0x2d9, 0xa8, 0, 0x2da, 0xb8, 0, 0x2dd, 0x2db, 0x2c7,
  0x2014, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0xc6, 0, 0xaa, 0, 0, 0, 0, 0x141, 0xd8, 0x152, 0xba, 0, 0, 0, 0,
  0, 0xe6, 0, 0, 0, 0x131, 0, 0, 0x142, 0xf8, 0x153, 0xdf, 0, 0, 0, 0,
];

// PDFDocEncoding's 0x80–0xa0; the rest is Latin-1.
const pdfDocHigh = [
  0x2022, 0x2020, 0x2021, 0x2026, 0x2014, 0x2013, 0x192, 0x2044, 0x2039, 0x203a, 0x2212, 0x2030, 0x201e, 0x201c, 0x201d, 0x2018,
  0x2019, 0x201a, 0x2122, 0xfb01, 0xfb02, 0x141, 0x152, 0x160, 0x178, 0x17d, 0x131, 0x142, 0x153, 0x161, 0x17e, 0, 0x20ac,
];

// Glyph names and their characters, in hex: the names the standard Latin
// encodings use, and those of Latin Extended-A. Single letters are
// themselves and are not listed.
const glyphNames =
  "space 20 exclam 21 quotedbl 22 numbersign 23 dollar 24 percent 25 ampersand 26 quotesingle 27 quoteright 2019 " +
  "parenleft 28 parenright 29 asterisk 2A plus 2B comma 2C hyphen 2D period 2E slash 2F zero 30 one 31 two 32 " +
  "three 33 four 34 five 35 six 36 seven 37 eight 38 nine 39 colon 3A semicolon 3B less 3C equal 3D greater 3E " +
  "question 3F at 40 bracketleft 5B backslash 5C bracketright 5D asciicircum 5E underscore 5F grave 60 " +
  "quoteleft 2018 braceleft 7B bar 7C braceright 7D asciitilde 7E nbspace A0 nonbreakingspace A0 exclamdown A1 " +
  "cent A2 sterling A3 currency A4 yen A5 brokenbar A6 section A7 dieresis A8 copyright A9 ordfeminine AA " +
  "guillemotleft AB logicalnot AC sfthyphen AD registered AE macron AF degree B0 plusminus B1 twosuperior B2 " +
  "threesuperior B3 acute B4 mu B5 paragraph B6 periodcentered B7 cedilla B8 onesuperior B9 ordmasculine BA " +
  "guillemotright BB onequarter BC onehalf BD threequarters BE questiondown BF Agrave C0 Aacute C1 " +
  "Acircumflex C2 Atilde C3 Adieresis C4 Aring C5 AE C6 Ccedilla C7 Egrave C8 Eacute C9 Ecircumflex CA " +
  "Edieresis CB Igrave CC Iacute CD Icircumflex CE Idieresis CF Eth D0 Ntilde D1 Ograve D2 Oacute D3 " +
  "Ocircumflex D4 Otilde D5 Odieresis D6 multiply D7 Oslash D8 Ugrave D9 Uacute DA Ucircumflex DB Udieresis DC " +
  "Yacute DD Thorn DE germandbls DF agrave E0 aacute E1 acircumflex E2 atilde E3 adieresis E4 aring E5 ae E6 " +
  "ccedilla E7 egrave E8 eacute E9 ecircumflex EA edieresis EB igrave EC iacute ED icircumflex EE idieresis EF " +
  "eth F0 ntilde F1 ograve F2 oacute F3 ocircumflex F4 otilde F5 odieresis F6 divide F7 oslash F8 ugrave F9 " +
  "uacute FA ucircumflex FB udieresis FC yacute FD thorn FE ydieresis FF Amacron 100 amacron 101 Abreve 102 " +
  "abreve 103 Aogonek 104 aogonek 105 Cacute 106 cacute 107 Ccaron 10C ccaron 10D Dcaron 10E dcaron 10F " +
  "Dcroat 110 dcroat 111 Emacron 112 emacron 113 Edotaccent 116 edotaccent 117 Eogonek 118 eogonek 119 " +
  "Ecaron 11A ecaron 11B Gbreve 11E gbreve 11F Idotaccent 130 dotlessi 131 Lacute 139 lacute 13A Lcaron 13D " +
  "lcaron 13E Lslash 141 lslash 142 Nacute 143 nacute 144 Ncaron 147 ncaron 148 Ohungarumlaut 150 " +
  "ohungarumlaut 151 OE 152 oe 153 Racute 154 racute 155 Rcaron 158 rcaron 159 Sacute 15A sacute 15B " +
  "Scedilla 15E scedilla 15F Scaron 160 scaron 161 Tcaron 164 tcaron 165 Uring 16E uring 16F Uhungarumlaut 170 " +
  "uhungarumlaut 171 Ydieresis 178 Zacute 179 zacute 17A Zdotaccent 17B zdotaccent 17C Zcaron 17D zcaron 17E " +
  "florin 192 circumflex 2C6 caron 2C7 breve 2D8 dotaccent 2D9 ring 2DA ogonek 2DB tilde 2DC hungarumlaut 2DD " +
  "pi 3C0 endash 2013 emdash 2014 quotesinglbase 201A quotedblleft 201C quotedblright 201D quotedblbase 201E " +
  "dagger 2020 daggerdbl 2021 bullet 2022 ellipsis 2026 perthousand 2030 guilsinglleft 2039 guilsinglright 203A " +
  "fraction 2044 Euro 20AC trademark 2122 Omega 2126 partialdiff 2202 Delta 2206 product 220F summation 2211 " +
  "minus 2212 radical 221A infinity 221E integral 222B approxequal 2248 notequal 2260 lessequal 2264 " +
  "greaterequal 2265 lozenge 25CA ff FB00 fi FB01 fl FB02 ffi FB03 ffl FB04";

function table(high, from) {
  const codes = new Array(256).fill(0);
  for (let code = 0x20; code < 0x7f; code++) codes[code] = code;
  high.forEach((value, index) => (codes[from + index] = value));
  return codes;
}

const winAnsi = table(winAnsiHigh, 0x80);
for (let code = 0xa0; code <= 0xff; code++) winAnsi[code] = code;
const macRoman = table(macRomanHigh, 0x80);
const standard = table(standardHigh, 0xa0);
standard[0x27] = 0x2019;
standard[0x60] = 0x2018;

const names = new Map();
const list = glyphNames.split(" ");
for (let i = 0; i + 1 < list.length; i += 2) names.set(list[i], parseInt(list[i + 1], 16));

const hex = (digits) => (/^[0-9A-Fa-f]+$/.test(digits) ? parseInt(digits, 16) : 0);

/**
 * The code points of a simple font's 256 codes under a base encoding —
 * WinAnsiEncoding, MacRomanEncoding, or StandardEncoding for any other
 * name; 0 where a code has no character.
 */
export function baseEncoding(name) {
  if (name === "WinAnsiEncoding") return winAnsi;
  if (name === "MacRomanEncoding") return macRoman;
  return standard;
}

/**
 * The code point a glyph name stands for: the standard Latin names,
 * `uniXXXX` and `uXXXX`; 0 when it is not known. A name with a suffix,
 * `a.sc`, is its base name's character.
 */
export function glyphCode(full) {
  const name = full.split(".")[0];
  if (/^[A-Za-z]$/.test(name)) return name.charCodeAt(0);
  if (names.has(name)) return names.get(name);
  if (name.length === 7 && name.startsWith("uni")) return hex(name.slice(3));
  if (name.length >= 5 && name.length <= 7 && name[0] === "u") return hex(name.slice(1));
  return 0;
}

const ligatures = ["ff", "fi", "fl", "ffi", "ffl", "ft", "st"];

/** A character as text; a ligature, `ﬁ`, as its letters, so the words it is in can be searched. */
export function characterText(code) {
  if (code >= 0xfb00 && code <= 0xfb06) return ligatures[code - 0xfb00];
  if (!code || code > 0x10ffff || (code >= 0xd800 && code < 0xe000)) return "";
  return String.fromCodePoint(code);
}

/** Text from UTF-16 bytes, as a binary string. */
export function fromUtf16(bytes) {
  let text = "";
  for (let i = 0; i + 1 < bytes.length; i += 2) {
    let unit = (bytes.charCodeAt(i) << 8) | bytes.charCodeAt(i + 1);
    if (unit >= 0xd800 && unit < 0xdc00 && i + 3 < bytes.length) {
      const low = (bytes.charCodeAt(i + 2) << 8) | bytes.charCodeAt(i + 3);
      unit = 0x10000 + ((unit - 0xd800) << 10) + (low - 0xdc00);
      i += 2;
    }
    text += characterText(unit);
  }
  return text;
}

/**
 * A text string — a title, a bookmark — as text: UTF-16 after its byte
 * order mark, UTF-8 after its own, or else PDFDocEncoding. Trimmed.
 */
export function textString(bytes) {
  let text = "";
  if (bytes.startsWith("\xfe\xff")) {
    text = fromUtf16(bytes.slice(2));
  } else if (bytes.startsWith("\xef\xbb\xbf")) {
    text = new TextDecoder().decode(Uint8Array.from(bytes.slice(3), (c) => c.charCodeAt(0)));
  } else {
    for (const character of bytes) {
      const byte = character.charCodeAt(0);
      if (byte >= 0x80 && byte <= 0xa0) text += characterText(pdfDocHigh[byte - 0x80]);
      else if (byte >= 0x20 || byte === 0x09 || byte === 0x0a || byte === 0x0d) text += character;
    }
  }
  // Titles are often padded, or end in a stray terminator.
  return text.trim();
}
