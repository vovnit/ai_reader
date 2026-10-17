// The text of a book file in whichever encoding it came in. FB2 and XHTML
// declare theirs in the XML prolog and HTML in a meta tag; a .txt says
// nothing, so a file that is not UTF-8 is read as windows-1251 when more of
// its letters are Cyrillic than Latin, else as windows-1252. The tables are
// the browsers' own, so the Kindle and iOS apps, which carry the same
// tables, read a file as a page does. The same as `Support/Charset.cpp`
// (Kindle) and `Support/Charset.swift` (iOS).
import { decode as decodeUnicode } from "./TextFile.js";

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
// Code points for bytes 0x80–0x9F; from 0xA0 the byte is the code point.
const cp1252 = [
  0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
  0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
  0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
  0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
];

const cyrillicNames = ["windows-1251", "cp1251", "x-cp1251"];
const westernNames = ["windows-1252", "cp1252", "iso-8859-1", "latin1", "us-ascii", "ascii"];

function singleByte(bytes, high) {
  let text = "";
  for (let i = 0; i < bytes.length; i += 4096) {
    const codes = Array.from(bytes.subarray(i, i + 4096), (b) => (b < 0x80 ? b : high(b)));
    text += String.fromCodePoint(...codes);
  }
  return text;
}

const windows1251 = (bytes) => singleByte(bytes, (b) => cp1251[b - 0x80]);
const windows1252 = (bytes) => singleByte(bytes, (b) => (b < 0xa0 ? cp1252[b - 0x80] : b));

function isUtf8(bytes) {
  try {
    new TextDecoder("utf-8", { fatal: true }).decode(bytes);
    return true;
  } catch {
    return false;
  }
}

/** More letter bytes in 0xC0–0xFF, where windows-1251 keeps А–я, than ASCII letters. */
function looksCyrillic(bytes) {
  let high = 0;
  let ascii = 0;
  for (const b of bytes) {
    if (b >= 0xc0) high++;
    else if ((b >= 0x41 && b <= 0x5a) || (b >= 0x61 && b <= 0x7a)) ascii++;
  }
  return high > ascii;
}

/** The encoding a file names in its first bytes: `<?xml encoding="…"?>` or `<meta charset="…">`; "" when none. */
export function declaredCharset(bytes) {
  const head = String.fromCharCode(...bytes.subarray(0, 1024));
  const match = /<\?xml[^>]*encoding\s*=\s*["']([^"']+)/i.exec(head) ?? /<meta[^>]*charset\s*=\s*["']?([\w-]+)/i.exec(head);
  return match ? match[1].trim().toLowerCase() : "";
}

/** `bytes` as text; `declared` is the encoding the file names, if any. */
export function decodeBook(bytes, declared = "") {
  const bom = (bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) || (bytes[0] === 0xff && bytes[1] === 0xfe) || (bytes[0] === 0xfe && bytes[1] === 0xff);
  // UTF-16 without a mark shows itself by its zero bytes, which UTF-8 also allows.
  if (bom || (bytes.length > 1 && (bytes[0] === 0 || bytes[1] === 0))) return decodeUnicode(bytes);
  if (cyrillicNames.includes(declared)) return windows1251(bytes);
  if (westernNames.includes(declared)) return windows1252(bytes);
  if (isUtf8(bytes)) return new TextDecoder().decode(bytes);
  return looksCyrillic(bytes) ? windows1251(bytes) : windows1252(bytes);
}
