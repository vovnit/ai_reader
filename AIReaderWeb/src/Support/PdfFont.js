// How a font turns the bytes a page shows into text, and how far each of
// its glyphs moves the pen. The font's ToUnicode map is trusted first; a
// simple font without one falls back on its encoding's glyph names. The
// same reading as the Kindle app's `PdfFont`.
import { baseEncoding, characterText, fromUtf16, glyphCode } from "./PdfEncodings.js";
import { binary, isName, isStream, PdfLexer } from "./PdfObject.js";

/** A code's bytes as a number, big-endian. */
function value(bytes) {
  let code = 0;
  for (let i = 0; i < bytes.length; i++) code = code * 256 + bytes.charCodeAt(i);
  return code;
}

/** `base`, a UTF-16 string, with its last unit moved on by `step`: how a `bfrange` numbers the characters of its codes. */
function stepped(base, step) {
  if (base.length < 2) return base;
  const last = value(base.slice(-2)) + step;
  return base.slice(0, -2) + String.fromCharCode((last >> 8) & 0xff, last & 0xff);
}

/** A single byte, against the rule, is taken as the character itself. */
const unicodeText = (bytes) => (bytes.length === 1 ? characterText(bytes.charCodeAt(0)) : fromUtf16(bytes));

export class PdfFont {
  composite = false;
  /** `{ bytes, low, high }`: how many bytes a composite font's codes take. */
  codeRanges = [];
  unicode = new Map();
  simple = baseEncoding("WinAnsiEncoding");
  widths = new Map();
  defaultWidth = 500;
  /** Type 3 glyph widths are in the font's own units. */
  widthScale = 1;

  /** A font from its dictionary; without one, one byte a code, as WinAnsiEncoding reads it. */
  static async read(document, font) {
    const read = new PdfFont();
    if (!font) return read;
    read.composite = isName(document.get(font, "Subtype"), "Type0");
    const toUnicode = document.get(font, "ToUnicode");
    if (read.composite) {
      // The encoding's code ranges say how many bytes each code takes; a
      // named one, Identity-H, takes two.
      const encoding = document.get(font, "Encoding");
      if (isStream(encoding)) read.#readCMap(binary(await document.contents(encoding)));
      const fromEncoding = read.codeRanges;
      if (isStream(toUnicode)) read.#readCMap(binary(await document.contents(toUnicode)));
      read.codeRanges = fromEncoding.length ? fromEncoding : [{ bytes: 2, low: 0, high: 0xffff }];
    } else {
      read.#readEncoding(document, font);
      if (isStream(toUnicode)) read.#readCMap(binary(await document.contents(toUnicode)));
    }
    read.#readWidths(document, font);
    return read;
  }

  #readCMap(cmap) {
    const lexer = new PdfLexer(Uint8Array.from(cmap, (c) => c.charCodeAt(0)));
    let operands = [];
    while (!lexer.atEnd()) {
      const token = lexer.next();
      if (token?.kind !== "op") {
        operands.push(token);
        continue;
      }
      if (token.text === "endcodespacerange") {
        for (let i = 0; i + 1 < operands.length; i += 2) {
          const low = operands[i]?.text ?? "";
          if (low.length && low.length <= 4) this.codeRanges.push({ bytes: low.length, low: value(low), high: value(operands[i + 1]?.text ?? "") });
        }
        this.codeRanges.sort((a, b) => a.bytes - b.bytes);
      } else if (token.text === "endbfchar") {
        for (let i = 0; i + 1 < operands.length; i += 2) {
          const target = operands[i + 1];
          this.unicode.set(value(operands[i]?.text ?? ""), target?.kind === "name" ? "" : unicodeText(target?.text ?? ""));
        }
      } else if (token.text === "endbfrange") {
        for (let i = 0; i + 2 < operands.length; i += 3) {
          const low = value(operands[i]?.text ?? "");
          const high = value(operands[i + 1]?.text ?? "");
          const target = operands[i + 2];
          if (high < low || high - low > 0xffff) continue;
          for (let code = low; code <= high; code++) {
            if (Array.isArray(target)) {
              if (code - low < target.length) this.unicode.set(code, unicodeText(target[code - low]?.text ?? ""));
            } else {
              this.unicode.set(code, unicodeText(stepped(target?.text ?? "", code - low)));
            }
          }
        }
      }
      operands = [];
    }
  }

  #readEncoding(document, font) {
    const encoding = document.get(font, "Encoding");
    let base = encoding?.kind === "name" ? encoding.text : document.get(encoding, "BaseEncoding")?.text ?? "";
    if (!base && isName(document.get(font, "Subtype"), "TrueType")) base = "WinAnsiEncoding";
    this.simple = [...baseEncoding(base)];
    let code = 0;
    for (const item of document.get(encoding, "Differences") ?? []) {
      const difference = document.resolve(item);
      if (typeof difference === "number") {
        code = difference;
      } else if (difference?.kind === "name") {
        if (code >= 0 && code < 256) this.simple[code] = glyphCode(difference.text);
        code++;
      }
    }
  }

  #readWidths(document, font) {
    const number = (item) => {
      const resolved = document.resolve(item);
      return typeof resolved === "number" ? resolved : 0;
    };
    if (this.composite) {
      const descendants = document.get(font, "DescendantFonts");
      const cid = Array.isArray(descendants) ? document.resolve(descendants[0]) : null;
      const fallback = document.get(cid, "DW");
      this.defaultWidth = typeof fallback === "number" ? fallback : 1000;
      const list = document.get(cid, "W") ?? [];
      for (let i = 0; i + 1 < list.length; ) {
        const first = number(list[i]);
        const next = document.resolve(list[i + 1]);
        if (Array.isArray(next)) {
          next.forEach((width, k) => this.widths.set(first + k, number(width)));
          i += 2;
        } else if (i + 2 < list.length) {
          const width = number(list[i + 2]);
          for (let code = first; code <= next && code - first <= 0xffff; code++) this.widths.set(code, width);
          i += 3;
        } else {
          break;
        }
      }
      return;
    }
    const first = number(document.get(font, "FirstChar"));
    const list = document.get(font, "Widths") ?? [];
    list.forEach((width, k) => this.widths.set(first + k, number(width)));
    const missing = document.get(document.get(font, "FontDescriptor"), "MissingWidth");
    if (typeof missing === "number" && missing > 0) this.defaultWidth = missing;
    // The standard fonts may come without widths; Courier's are all 600.
    else if (!list.length && (document.get(font, "BaseFont")?.text ?? "").includes("Courier")) this.defaultWidth = 600;
    const matrix = document.get(font, "FontMatrix");
    if (isName(document.get(font, "Subtype"), "Type3") && Array.isArray(matrix) && matrix.length) this.widthScale = number(matrix[0]) * 1000;
  }

  #codeLength(bytes, at) {
    for (const range of this.codeRanges) {
      if (at + range.bytes > bytes.length) continue;
      const code = value(bytes.slice(at, at + range.bytes));
      if (code >= range.low && code <= range.high) return range.bytes;
    }
    return Math.min(2, bytes.length - at);
  }

  /** `{ text, width, isSpace }` for each code in `bytes`, a binary string; the width in thousandths of the text size. */
  glyphs(bytes) {
    const glyphs = [];
    for (let at = 0; at < bytes.length; ) {
      const length = this.composite ? this.#codeLength(bytes, at) : 1;
      const code = value(bytes.slice(at, at + length));
      let text = this.unicode.get(code);
      if (text === undefined) text = this.composite ? "" : characterText(this.simple[code & 0xff]);
      glyphs.push({ text, width: (this.widths.get(code) ?? this.defaultWidth) * this.widthScale, isSpace: length === 1 && code === 32 });
      at += length;
    }
    return glyphs;
  }
}
