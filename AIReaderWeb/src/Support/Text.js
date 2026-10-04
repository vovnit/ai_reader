// Small string helpers, Unicode-aware where the other apps lean on GLib.

const space = /\s/u;
const punctuationOrSymbol = /[\p{P}\p{S}]/u;
const letterOrDigit = /[\p{L}\p{Nd}]/u;
const letter = /\p{L}/u;

/** Trims the ASCII whitespace the other apps' `Text::trim` trims. */
export function trim(text) {
  return text.replace(/^[ \t\r\n]+|[ \t\r\n]+$/g, "");
}

export function isSpace(character) {
  return space.test(character);
}

export function isPunctuationOrSymbol(character) {
  return punctuationOrSymbol.test(character);
}

export function isLetterOrDigit(character) {
  return letterOrDigit.test(character);
}

export function isLetter(character) {
  return letter.test(character);
}

/** True when nothing but whitespace and illustration placeholders is left. */
export function isBlank(text) {
  for (const character of text) {
    if (!isSpace(character) && character !== "￼") return false;
  }
  return true;
}

/**
 * Orders strings by code point, which is how the C++ apps order the same
 * strings as UTF-8 bytes; plain `<` compares UTF-16 units, which differs
 * past U+FFFF.
 */
export function compareCodePoints(a, b) {
  const length = Math.min(a.length, b.length);
  for (let i = 0; i < length; i++) {
    const x = a.codePointAt(i);
    const y = b.codePointAt(i);
    if (x !== y) return x < y ? -1 : 1;
    if (x > 0xffff) i++;
  }
  return a.length - b.length;
}

/** The time in the form every app writes: `2026-09-16T10:00:00Z`, which sorts as text. */
export function now() {
  return new Date().toISOString().replace(/\.\d{3}Z$/, "Z");
}

/** `text` cut to at most `limit` UTF-16 units, never inside a surrogate pair. */
export function cut(text, limit) {
  if (text.length <= limit) return text;
  let end = limit;
  const code = text.charCodeAt(end - 1);
  if (code >= 0xd800 && code <= 0xdbff) end--;
  return text.slice(0, end);
}
