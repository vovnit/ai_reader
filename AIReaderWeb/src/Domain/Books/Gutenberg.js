// A Project Gutenberg file wraps its book in a header, which names it, and
// the license: the book lies between the line "*** START OF …" and the line
// "*** END OF …", or "End of the Project Gutenberg EBook of …", which older
// files put before it. The plain-text reader hands its lines here, the HTML
// reader its paragraphs, so a book reads the same from either file. The
// same as `Gutenberg.cpp` (Kindle) and `Gutenberg.swift` (iOS).
import { collapsed } from "./TextChapters.js";

/** How far into the file the header's "Title:" and "Author:" lines are looked for. */
const headerLines = 60;

function isMarker(lower, words) {
  return lower.startsWith("***") && collapsed(lower.slice(3)).startsWith(words);
}

const isStart = (lower) => isMarker(lower, "start of");
const isEnd = (lower) => isMarker(lower, "end of") || lower.startsWith("end of the project gutenberg") || lower.startsWith("end of project gutenberg");

/**
 * `{ start, end, title, author }`: where the two markers are in `lines`,
 * and what the header names the book ("" when it does not); null when
 * `lines` are not a Gutenberg file's.
 */
export function gutenbergParts(lines) {
  const lowered = lines.map((line) => collapsed(line).toLowerCase());
  const start = lowered.findIndex(isStart);
  if (start < 0) return null;
  const end = lowered.findIndex((lower, i) => i > start && isEnd(lower));
  if (end < 0) return null;
  const field = (name) => {
    for (const line of lines.slice(0, Math.min(start, headerLines))) {
      const text = collapsed(line);
      if (text.startsWith(name)) return collapsed(text.slice(name.length));
    }
    return "";
  };
  return { start, end, title: field("Title:"), author: field("Author:") };
}
