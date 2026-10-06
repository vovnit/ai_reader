// Finds a query in a book's text, case-insensitively, and cuts a readable
// excerpt around every hit: `{ chapter, offset, excerpt, matchStart, matchEnd }`.
// Pure: no display, no disk. Offsets are UTF-16 units.
import { isSpace } from "../../Support/Text.js";

/** How far an excerpt reaches on either side of its match when the sentence runs longer. */
export const reach = 220;

const lower = (character) => character.toLowerCase();

/** Code point length of the character at `i`. */
function width(text, i) {
  const code = text.charCodeAt(i);
  return code >= 0xd800 && code <= 0xdbff && i + 1 < text.length ? 2 : 1;
}

function before(text, i) {
  const code = text.charCodeAt(i - 1);
  return code >= 0xdc00 && code <= 0xdfff && i > 1 ? i - 2 : i - 1;
}

/** Where the text at `at` stops reading as `wanted`, letter for letter ignoring case; -1 if it does not. */
function matchEnd(text, at, wanted) {
  let p = at;
  for (const letter of wanted) {
    if (p >= text.length) return -1;
    const step = width(text, p);
    if (lower(text.slice(p, p + step)) !== letter) return -1;
    p += step;
  }
  return p;
}

const endsSentence = (c) => c === "." || c === "!" || c === "?" || c === "…";

/** Where the sentence holding `offset` begins: after a paragraph break, or after the space that follows a full stop. */
function sentenceStart(text, offset, floor) {
  let p = offset;
  while (p > floor) {
    const previous = before(text, p);
    const c = text[previous];
    if (c === "\n") return p;
    if (isSpace(c) && previous > 0 && endsSentence(text[before(text, previous)])) return p;
    p = previous;
  }
  return p;
}

/** One past where the sentence holding `offset` ends: at a paragraph break, or a full stop followed by space or the end. */
function sentenceEnd(text, offset, ceiling) {
  for (let p = offset; p < ceiling; p += width(text, p)) {
    const c = text[p];
    if (c === "\n") return p;
    if (!endsSentence(c)) continue;
    let next = p + 1;
    // Closing quotes and brackets belong to the sentence.
    while (next < ceiling && "»\")”".includes(text[next])) next++;
    if (next >= ceiling || isSpace(text[next])) return next;
  }
  return ceiling;
}

/** Moves a cut point to the nearest space on the far side, so a cut never splits a word. */
function cutBackward(text, at, floor) {
  let p = at;
  while (p > floor && !isSpace(text[p])) p = before(text, p);
  return p;
}

function cutForward(text, at, ceiling) {
  let p = at;
  while (p < ceiling && !isSpace(text[p])) p += width(text, p);
  return p;
}

function excerptRange(text, start, end, distance) {
  let from = sentenceStart(text, start, Math.max(0, start - distance));
  let to = sentenceEnd(text, end, Math.min(text.length, end + distance));
  const front = from > 0 && text[from - 1] !== "\n" && start - from >= distance;
  const back = to < text.length && text[to] !== "\n" && to - end >= distance;
  if (front) from = cutForward(text, from, start);
  if (back) to = cutBackward(text, to, end);
  return { from, to, front, back };
}

/** The excerpt for a match at `[start, end)`: its sentence, or as much of it as fits within `reach`. */
export function excerpt(text, start, end, distance = reach) {
  const range = excerptRange(text, start, end, distance);
  let body = (range.front ? "…" : "") + text.slice(range.from, range.to) + (range.back ? "…" : "");
  const shift = range.front ? 1 : 0;
  let matchStart = start - range.from + shift;
  let matchEnd = end - range.from + shift;
  // Trim the whitespace a sentence boundary leaves behind.
  while (matchStart > 0 && (body[0] === " " || body[0] === "\t")) {
    body = body.slice(1);
    matchStart--;
    matchEnd--;
  }
  return { offset: start, excerpt: body.replace(/ +$/, ""), matchStart, matchEnd };
}

/** Hits in one chapter, in order, at most `limit`. A sentence with the query in it twice is one hit. */
export function findInText(text, query, chapter, limit) {
  const wanted = [...query].map(lower);
  const hits = [];
  if (!wanted.length || limit <= 0) return hits;
  // Where the last excerpt ended: a second match there is the same passage.
  let covered = 0;
  for (let p = 0; p < text.length && hits.length < limit; ) {
    const stop = matchEnd(text, p, wanted);
    if (stop < 0) {
      p += width(text, p);
      continue;
    }
    const start = p;
    p = stop;
    if (start < covered) continue;
    hits.push({ ...excerpt(text, start, stop), chapter });
    covered = excerptRange(text, start, stop, reach).to;
  }
  return hits;
}

/** How far one step past a passage reaches before it is evened out to a sentence. */
export const step = 1200;

/**
 * Where a step back from `start` begins: about `step` earlier, at the start
 * of a sentence — or of a word, when the sentence runs long. Never before
 * the start of the text.
 */
export function stepBack(text, start) {
  let target = Math.max(0, start - step);
  if (target === 0) return 0;
  const floor = Math.max(0, target - reach);
  const from = sentenceStart(text, target, floor);
  // The chapter starting within reach is a boundary of its own.
  if (from > floor || floor === 0) return from;
  // A sentence longer than `reach` is entered at a word; a text without
  // spaces, at a character.
  const word = cutForward(text, floor, start);
  if (word < start) return word;
  if (/[\udc00-\udfff]/.test(text[target])) target++;
  return target;
}

/** Where a step forward from `end` stops: about `step` later, at the end of a sentence or a word. Never past the end of the text. */
export function stepForward(text, end) {
  let target = Math.min(text.length, end + step);
  if (target === text.length) return target;
  const ceiling = Math.min(text.length, target + reach);
  const to = sentenceEnd(text, target, ceiling);
  if (to < ceiling || ceiling === text.length) return to;
  const word = cutBackward(text, ceiling, end);
  if (word > end) return word;
  if (/[\udc00-\udfff]/.test(text[target])) target--;
  return target;
}
