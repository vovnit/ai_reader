// The text of a book file in whichever encoding it came in. A byte-order
// mark settles it; UTF-16 without one shows itself by its zero bytes; then
// UTF-8 wins whenever most of its multi-byte sequences are valid, whatever
// the file declares, since declarations are often wrong and a stray bad
// byte should cost one character, not the book. Otherwise FB2 and XHTML
// name a single-byte encoding in the XML prolog and HTML in a meta tag, and
// a file that names none is guessed: Western, windows-1251 or, since lib.ru
// serves it undeclared, KOI8-R. Bad input is replaced as the browser's
// TextDecoder replaces it, and the tables are the browsers' own, so the
// Kindle and iOS apps, which carry the same decoders, read a file as a page
// does. The same as `Support/Charset.cpp` (Kindle) and `Support/Charset.swift`
// (iOS).

// Code points for bytes 0x80–0xFF.
const cp1251 = [
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
];
// Code points for bytes 0x80–0xFF; lib.ru still serves Russian in KOI8-R.
const koi8r = [
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
];
// Code points for bytes 0x80–0x9F; from 0xA0 the byte is the code point.
const cp1252 = [
  0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
  0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
  0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
  0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
];

const cyrillicNames = ["windows-1251", "cp1251", "x-cp1251"];
const koi8Names = ["koi8-r", "koi8-u"];
const westernNames = ["windows-1252", "cp1252", "iso-8859-1", "latin1", "us-ascii", "ascii"];

/**
 * Adds a decoded character as the EPUB can carry it: a byte-order mark
 * doubled at the start dropped, a form feed or vertical tab a line break (a
 * form-feed line is a page break, so it ends a paragraph), and the other
 * controls and the non-characters U+FFFE and U+FFFF, which XML forbids,
 * dropped.
 */
function add(code, codes) {
  if (code === 0xfeff && codes.length === 0) return;
  if (code === 0x0b || code === 0x0c) codes.push(0x0a);
  else if (code > 0x08 && (code < 0x0e || code > 0x1f) && code !== 0x7f && code !== 0xfffe && code !== 0xffff) codes.push(code);
}

function text(codes) {
  let result = "";
  for (let i = 0; i < codes.length; i += 4096) result += String.fromCodePoint(...codes.slice(i, i + 4096));
  return result;
}

/** UTF-8 as TextDecoder reads it, and how many multi-byte sequences were valid and how many replaced. */
function utf8(bytes) {
  const codes = [];
  let valid = 0;
  let invalid = 0;
  let code = 0;
  let needed = 0;
  let seen = 0;
  let lower = 0x80;
  let upper = 0xbf;
  for (let i = 0; i < bytes.length; i++) {
    const b = bytes[i];
    if (needed === 0) {
      if (b < 0x80) {
        add(b, codes);
      } else if (b >= 0xc2 && b <= 0xdf) {
        needed = 1;
        code = b & 0x1f;
      } else if (b >= 0xe0 && b <= 0xef) {
        // The bounds rule out overlong forms and surrogates.
        if (b === 0xe0) lower = 0xa0;
        if (b === 0xed) upper = 0x9f;
        needed = 2;
        code = b & 0x0f;
      } else if (b >= 0xf0 && b <= 0xf4) {
        if (b === 0xf0) lower = 0x90;
        if (b === 0xf4) upper = 0x8f;
        needed = 3;
        code = b & 0x07;
      } else {
        add(0xfffd, codes);
        invalid++;
      }
      continue;
    }
    if (b < lower || b > upper) {
      // The sequence so far is one replacement, and this byte starts afresh.
      add(0xfffd, codes);
      invalid++;
      needed = seen = 0;
      lower = 0x80;
      upper = 0xbf;
      i--;
      continue;
    }
    lower = 0x80;
    upper = 0xbf;
    code = (code << 6) | (b & 0x3f);
    if (++seen === needed) {
      add(code, codes);
      valid++;
      needed = seen = 0;
    }
  }
  if (needed) {
    add(0xfffd, codes);
    invalid++;
  }
  return { codes, valid, invalid };
}

/** UTF-16 from `start`, a lone surrogate or an odd last byte replaced as TextDecoder does. */
function utf16(bytes, start, bigEndian) {
  const codes = [];
  let high = 0;
  let i = start;
  for (; i + 1 < bytes.length; i += 2) {
    const unit = bigEndian ? (bytes[i] << 8) | bytes[i + 1] : (bytes[i + 1] << 8) | bytes[i];
    if (high) {
      if (unit >= 0xdc00 && unit <= 0xdfff) {
        add(0x10000 + ((high - 0xd800) << 10) + (unit - 0xdc00), codes);
        high = 0;
        continue;
      }
      add(0xfffd, codes);
      high = 0;
    }
    if (unit >= 0xd800 && unit <= 0xdbff) high = unit;
    else add(unit >= 0xdc00 && unit <= 0xdfff ? 0xfffd : unit, codes);
  }
  // A pair cut short by the end, an odd last byte, or both: one replacement.
  if (high || i < bytes.length) add(0xfffd, codes);
  return codes;
}

function singleByte(bytes, high) {
  const codes = [];
  for (const b of bytes) add(b < 0x80 ? b : high(b), codes);
  return codes;
}

/**
 * UTF-16 without a mark: its zero bytes fall at odd offsets (little-endian)
 * or even ones, in at least one pair in sixteen of the first 1024 bytes, and
 * hardly ever at the others, so a stray NUL in a UTF-8 file does not count,
 * nor a run of them padding one out.
 */
function unmarkedUtf16(bytes) {
  const pairs = Math.min(bytes.length, 1024) >> 1;
  let even = 0;
  let odd = 0;
  for (let i = 0; i < pairs * 2; i++) {
    if (bytes[i] === 0) i % 2 ? odd++ : even++;
  }
  const zeros = Math.max(even, odd);
  if (zeros === 0 || zeros * 16 < pairs || Math.min(even, odd) * 4 >= zeros) return "";
  return odd > even ? "le" : "be";
}

const isLetter = (b) => (b >= 0x41 && b <= 0x5a) || (b >= 0x61 && b <= 0x7a);

/**
 * Cyrillic words in windows-1251 are runs of bytes from 0xC0, where А–я
 * are; a Western accent sits inside a Latin word. Markup and base64 have
 * no high bytes, so they do not sway it.
 */
function looksCyrillic(bytes) {
  let runs = 0;
  let mixed = 0;
  for (let i = 1; i < bytes.length; i++) {
    const a = bytes[i - 1];
    const b = bytes[i];
    if (a >= 0xc0 && b >= 0xc0) runs++;
    else if ((a >= 0xc0 && isLetter(b)) || (b >= 0xc0 && isLetter(a))) mixed++;
  }
  return runs > mixed;
}

/**
 * Most letters are lower case, and KOI8-R puts а–я at 0xC0–0xDF where
 * windows-1251 has А–Я, its own а–я being at 0xE0–0xFF.
 */
function looksKoi8(bytes) {
  let lower = 0;
  let upper = 0;
  for (const b of bytes) {
    if (b >= 0xe0) upper++;
    else if (b >= 0xc0) lower++;
  }
  return lower > upper;
}

/** The encoding a file names in its first bytes: `<?xml encoding="…"?>` or `<meta charset="…">`, lowercased; "" when none. */
export function declaredCharset(bytes) {
  // ASCII only, lowercased, so every app reads the same name.
  const head = String.fromCharCode(...Array.from(bytes.subarray(0, 1024), (b) => (b >= 0x80 ? 0x3f : b >= 0x41 && b <= 0x5a ? b + 0x20 : b)));
  const match = /<\?xml[^>]*encoding[ \t\r\n]*=[ \t\r\n]*["']([^"']+)/.exec(head) ?? /<meta[^>]*charset[ \t\r\n]*=[ \t\r\n]*["']?([a-z0-9_-]+)/.exec(head);
  return match ? match[1].replace(/^[ \t\r\n]+|[ \t\r\n]+$/g, "") : "";
}

/** `bytes` as text; `declared` is the encoding the file names, if any. */
export function decodeBook(bytes, declared = "") {
  return text(codePoints(bytes, declared));
}

function codePoints(bytes, declared) {
  if (bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) return utf8(bytes.subarray(3)).codes;
  if (bytes[0] === 0xff && bytes[1] === 0xfe) return utf16(bytes, 2, false);
  if (bytes[0] === 0xfe && bytes[1] === 0xff) return utf16(bytes, 2, true);
  const unmarked = unmarkedUtf16(bytes);
  if (unmarked) return utf16(bytes, 0, unmarked === "be");
  const unicode = utf8(bytes);
  if (unicode.valid > unicode.invalid) return unicode.codes;
  const windows1251 = (b) => cp1251[b - 0x80];
  const windows1252 = (b) => (b < 0xa0 ? cp1252[b - 0x80] : b);
  const koi8 = (b) => koi8r[b - 0x80];
  if (cyrillicNames.includes(declared)) return singleByte(bytes, windows1251);
  if (koi8Names.includes(declared)) return singleByte(bytes, koi8);
  if (westernNames.includes(declared)) return singleByte(bytes, windows1252);
  if (!looksCyrillic(bytes)) return singleByte(bytes, windows1252);
  return singleByte(bytes, looksKoi8(bytes) ? koi8 : windows1251);
}
