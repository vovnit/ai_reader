// A page's lines run together as the page reads — a photo's, or a PDF
// page's in the page view — so the sentence around a word can be found
// across line breaks. Each line is joined the way a PDF's lines are, a word
// hyphenated at a line's end mended, and stands whole at its start in the
// text, so the character at column c of line i is at `lineStarts[i] + c`.
// The same as the iOS app's `PagePassage`.
import { joined } from "./PdfParagraphs.js";

/** `{ text, lineStarts }` for the lines, in reading order. */
export function pagePassage(lines) {
  let text = "";
  const lineStarts = [];
  for (const line of lines) {
    // An empty line, as an item ending a line leaves after it, adds nothing.
    if (line === "") {
      lineStarts.push(text.length);
      continue;
    }
    text = text === "" ? line : joined(text, line);
    lineStarts.push(text.length - line.length);
  }
  return { text, lineStarts };
}

/**
 * A PDF page's lines from PDF.js's text items in order, an item marked
 * `hasEOL` ending its line: `{ lines, places }`, where `places[k]` is the
 * line and column item k's text starts at, so a place in an item is a
 * place in the passage.
 */
export function itemLines(items) {
  const lines = [""];
  const places = [];
  for (const item of items) {
    places.push({ line: lines.length - 1, column: lines.at(-1).length });
    lines[lines.length - 1] += item.str;
    if (item.hasEOL) lines.push("");
  }
  return { lines, places };
}
