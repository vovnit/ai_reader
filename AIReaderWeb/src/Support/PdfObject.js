// PDF syntax: the objects of a file, or the operands and operators of a
// content stream, read from bytes. A value is a number, a boolean, null, an
// array, or `{ kind }` — "name", "string" (its bytes as a binary string),
// "ref" (`number`), "op" (an operator), "dict" (`members`, a Map), or
// "stream" (`members`, and where its encoded bytes lie: `start`, `length`).
// Malformed input yields what could be read, never an exception.

const isSpace = (byte) => byte === 0x20 || byte === 0x0a || byte === 0x0d || byte === 0x09 || byte === 0x0c || byte === 0x00;
const delimiters = new Uint8Array(256);
for (const character of "()<>[]{}/%") delimiters[character.charCodeAt(0)] = 1;
const isDelimiterByte = (byte) => delimiters[byte] === 1;
const isDigit = (byte) => byte >= 0x30 && byte <= 0x39;

function hexValue(byte) {
  if (byte >= 0x30 && byte <= 0x39) return byte - 0x30;
  if (byte >= 0x61 && byte <= 0x66) return byte - 0x61 + 10;
  if (byte >= 0x41 && byte <= 0x46) return byte - 0x41 + 10;
  return -1;
}

/** Nesting deeper than this is not a book's; it is a broken or hostile file. */
const depthLimit = 64;

/** Bytes as a binary string, a character a byte, in pieces small enough for `fromCharCode`. */
export function binary(bytes) {
  let text = "";
  for (let i = 0; i < bytes.length; i += 8192) text += String.fromCharCode(...bytes.subarray(i, i + 8192));
  return text;
}

export const isName = (value, name) => value?.kind === "name" && value.text === name;
export const isDict = (value) => value?.kind === "dict";
export const isStream = (value) => value?.kind === "stream";

/** A dictionary's or stream's member, unresolved; null when missing. */
export function member(value, key) {
  return value?.members?.get(key) ?? null;
}

export class PdfLexer {
  #depth = 0;

  constructor(bytes, position = 0) {
    this.bytes = bytes;
    this.position = position;
  }

  #isDelimiter(at) {
    return at >= this.bytes.length || isSpace(this.bytes[at]) || isDelimiterByte(this.bytes[at]);
  }

  #skipSpace() {
    const bytes = this.bytes;
    while (this.position < bytes.length) {
      const byte = bytes[this.position];
      if (isSpace(byte)) {
        this.position++;
      } else if (byte === 0x25) {
        while (this.position < bytes.length && bytes[this.position] !== 0x0a && bytes[this.position] !== 0x0d) this.position++;
      } else {
        break;
      }
    }
  }

  /** True once only whitespace and comments are left. */
  atEnd() {
    this.#skipSpace();
    return this.position >= this.bytes.length;
  }

  /** The next value; a bare keyword comes back as an operator. */
  next() {
    this.#skipSpace();
    const bytes = this.bytes;
    if (this.position >= bytes.length) return null;
    const byte = bytes[this.position];
    if (isDigit(byte) || byte === 0x2d || byte === 0x2b || byte === 0x2e) return this.#number();
    if (byte === 0x28) return this.#literalString();
    if (byte === 0x2f) return this.#name();
    if (byte === 0x5b) return this.#array();
    if (byte === 0x3c) return bytes[this.position + 1] === 0x3c ? this.#dictionary() : this.#hexString();
    if (isDelimiterByte(byte)) {
      // A stray `]`, `>>` or brace: handed back as an operator to be ignored.
      this.position++;
      if (byte === 0x3e && bytes[this.position] === 0x3e) this.position++;
      return { kind: "op", text: String.fromCharCode(byte) };
    }
    const word = this.#keyword();
    if (word === "true" || word === "false") return word === "true";
    if (word === "null") return null;
    return { kind: "op", text: word };
  }

  #keyword() {
    const start = this.position;
    while (!this.#isDelimiter(this.position)) this.position++;
    if (this.position === start) this.position++; // never stall on an unexpected byte
    return binary(this.bytes.subarray(start, this.position));
  }

  #number() {
    const bytes = this.bytes;
    const start = this.position;
    if (bytes[this.position] === 0x2d || bytes[this.position] === 0x2b) this.position++;
    let whole = true;
    while (this.position < bytes.length && (isDigit(bytes[this.position]) || bytes[this.position] === 0x2e)) {
      whole &&= bytes[this.position] !== 0x2e;
      this.position++;
    }
    const value = parseFloat(binary(bytes.subarray(start, this.position))) || 0;
    // `12 0 R` is a reference to object 12.
    if (whole && isDigit(bytes[start])) {
      const after = this.position;
      this.#skipSpace();
      const generation = this.position;
      while (this.position < bytes.length && isDigit(bytes[this.position])) this.position++;
      if (this.position > generation && this.#isDelimiter(this.position)) {
        this.#skipSpace();
        if (bytes[this.position] === 0x52 && this.#isDelimiter(this.position + 1)) {
          this.position++;
          return { kind: "ref", number: value };
        }
      }
      this.position = after;
    }
    return value;
  }

  #literalString() {
    const bytes = this.bytes;
    const out = [];
    this.position++;
    let nesting = 1;
    const escapes = { 0x6e: 0x0a, 0x72: 0x0d, 0x74: 0x09, 0x62: 0x08, 0x66: 0x0c };
    while (this.position < bytes.length) {
      let byte = bytes[this.position++];
      if (byte === 0x28) {
        nesting++;
      } else if (byte === 0x29) {
        if (--nesting === 0) break;
      } else if (byte === 0x5c && this.position < bytes.length) {
        const escaped = bytes[this.position++];
        if (escaped in escapes) {
          out.push(escapes[escaped]);
        } else if (escaped === 0x0d) {
          if (bytes[this.position] === 0x0a) this.position++;
        } else if (escaped === 0x0a) {
          // a line break escaped away
        } else if (escaped >= 0x30 && escaped <= 0x37) {
          let value = escaped - 0x30;
          for (let digits = 1; digits < 3 && bytes[this.position] >= 0x30 && bytes[this.position] <= 0x37; digits++) {
            value = value * 8 + (bytes[this.position++] - 0x30);
          }
          out.push(value & 0xff);
        } else {
          out.push(escaped);
        }
        continue;
      } else if (byte === 0x0d) {
        // An end of line inside a string reads as a bare line feed.
        if (bytes[this.position] === 0x0a) this.position++;
        byte = 0x0a;
      }
      out.push(byte);
    }
    return { kind: "string", text: binary(Uint8Array.from(out)) };
  }

  #hexString() {
    const bytes = this.bytes;
    let text = "";
    let high = -1;
    this.position++;
    while (this.position < bytes.length && bytes[this.position] !== 0x3e) {
      const value = hexValue(bytes[this.position++]);
      if (value < 0) continue;
      if (high < 0) {
        high = value;
      } else {
        text += String.fromCharCode(high * 16 + value);
        high = -1;
      }
    }
    if (high >= 0) text += String.fromCharCode(high * 16);
    this.position++;
    return { kind: "string", text };
  }

  #name() {
    const bytes = this.bytes;
    const out = [];
    this.position++;
    while (!this.#isDelimiter(this.position)) {
      let byte = bytes[this.position++];
      if (byte === 0x23 && hexValue(bytes[this.position]) >= 0 && hexValue(bytes[this.position + 1]) >= 0) {
        byte = hexValue(bytes[this.position]) * 16 + hexValue(bytes[this.position + 1]);
        this.position += 2;
      }
      out.push(byte);
    }
    return { kind: "name", text: binary(Uint8Array.from(out)) };
  }

  #array() {
    const items = [];
    this.position++;
    if (++this.#depth > depthLimit) {
      this.#depth--;
      return items;
    }
    while (!this.atEnd()) {
      if (this.bytes[this.position] === 0x5d) {
        this.position++;
        break;
      }
      items.push(this.next());
    }
    this.#depth--;
    return items;
  }

  #dictionary() {
    const members = new Map();
    this.position += 2;
    if (++this.#depth > depthLimit) {
      this.#depth--;
      return { kind: "dict", members };
    }
    const closes = () => this.bytes[this.position] === 0x3e && this.bytes[this.position + 1] === 0x3e;
    while (!this.atEnd()) {
      if (closes()) {
        this.position += 2;
        break;
      }
      const key = this.next();
      if (key?.kind !== "name") continue;
      // A key with no value before the end reads as null.
      this.#skipSpace();
      if (closes()) continue;
      members.set(key.text, this.next());
    }
    this.#depth--;
    return { kind: "dict", members };
  }

  /** Steps over an inline image's data, from just after its `ID` to just after its `EI`. */
  skipInlineImage() {
    const bytes = this.bytes;
    if (isSpace(bytes[this.position])) this.position++;
    while (this.position + 1 < bytes.length) {
      if (bytes[this.position] === 0x45 && bytes[this.position + 1] === 0x49 && isSpace(bytes[this.position - 1])
        && this.#isDelimiter(this.position + 2)) {
        this.position += 2;
        return;
      }
      this.position++;
    }
    this.position = bytes.length;
  }
}
