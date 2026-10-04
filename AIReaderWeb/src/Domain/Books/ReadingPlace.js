// Where a reader is in a book, in terms that mean the same on every device:
// the chapter, how far into it, and the words at that point. Offsets differ
// between apps — UTF-16 units here and on iOS, bytes on the Kindle — and
// between renderings, so a place is found again by its words, and by its
// fraction when the words cannot be found.
//
// The words are compared loosely: every run of whitespace counts as one
// space, and illustration placeholders do not count at all, since the apps
// render those differently.
import { isSpace } from "../../Support/Text.js";

/** How much text is kept as the snippet, in UTF-16 units, before it is cut back to a word boundary. */
export const snippetLength = 80;

/** `text` with whitespace collapsed and placeholders dropped, and for each unit of the result the offset it came from. */
function normalize(text) {
  let out = "";
  const map = [];
  let pendingSpace = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === "￼") continue;
    if (isSpace(c)) {
      pendingSpace = out.length > 0;
      continue;
    }
    if (pendingSpace) {
      out += " ";
      map.push(i);
      pendingSpace = false;
    }
    out += c;
    map.push(i);
  }
  return { text: out, map };
}

/** The place at `offset` of a chapter's text: `{ chapter, fraction, snippet }`. */
export function placeAt(chapter, chapterText, offset) {
  const at = Math.min(Math.max(offset, 0), chapterText.length);
  const fraction = chapterText.length > 0 ? at / chapterText.length : 0;
  const normalized = normalize(chapterText.slice(at, at + snippetLength * 3)).text;
  let end = Math.min(normalized.length, snippetLength);
  // Cut back to a space so the snippet is whole words, unless that would leave nothing.
  if (end < normalized.length) {
    let cut = end;
    while (cut > 0 && normalized[cut - 1] !== " ") cut--;
    if (cut > 0) end = cut;
  }
  return { chapter, fraction, snippet: normalized.slice(0, end).trim() };
}

/** The offset in the chapter's text a place stands for: the snippet's occurrence nearest the fraction, else the fraction. */
export function resolvePlace(place, chapterText) {
  const fraction = Math.min(Math.max(place.fraction, 0), 1);
  const byFraction = () => {
    let offset = Math.floor(chapterText.length * fraction);
    // Never land inside a surrogate pair.
    const code = chapterText.charCodeAt(offset);
    if (offset > 0 && code >= 0xdc00 && code <= 0xdfff) offset--;
    return offset;
  };
  if (!place.snippet) return byFraction();
  const { text, map } = normalize(chapterText);
  const expected = text.length * fraction;
  let best = -1;
  for (let found = text.indexOf(place.snippet); found >= 0; found = text.indexOf(place.snippet, found + 1)) {
    if (best < 0 || Math.abs(found - expected) < Math.abs(best - expected)) best = found;
  }
  return best < 0 || best >= map.length ? byFraction() : map[best];
}

export function samePlace(a, b) {
  return !!a && !!b && a.chapter === b.chapter && a.fraction === b.fraction && a.snippet === b.snippet;
}
