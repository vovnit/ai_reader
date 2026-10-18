// Reads a plain-text book, already decoded, into `{ title, author, language,
// chapters }`. Text wrapped by hand, with blank lines between paragraphs,
// is joined back into paragraphs; text without blank lines has one
// paragraph per line. A short paragraph that names a chapter or is only a
// number opens a chapter. The same as `TextBook.cpp` (Kindle) and
// `TextBook.swift` (iOS).
import { gutenbergParts } from "./Gutenberg.js";
import { collapsed, isBlank, textChapters } from "./TextChapters.js";

/** The longest paragraph, in code points, still read as a chapter's heading. */
const headingLength = 80;

// Words a chapter heading begins with, lowercased: English, French,
// Spanish, Italian, German, Russian, Ukrainian.
const chapterWords = [
  "chapter", "part", "book", "prologue", "epilogue",
  "chapitre", "partie", "livre", "épilogue",
  "capítulo", "capitulo", "parte", "libro",
  "kapitel", "teil", "buch",
  "глава", "часть", "книга", "пролог", "эпилог", "розділ",
];

const isDigits = (text) => text !== "" && [...text].every((c) => c >= "0" && c <= "9");
const isRoman = (text) => text !== "" && [...text].every((c) => "IVXLCDM".includes(c));

/** "CHAPTER I.", "Глава 3", "XIV", "12.": kept simple, since a heading one app read differently would move every chapter after it. */
export function opensChapter(text) {
  if ([...text].length > headingLength) return false;
  const number = text.endsWith(".") ? text.slice(0, -1) : text;
  if (isDigits(number) || isRoman(number)) return true;
  const lower = text.toLowerCase();
  return chapterWords.some((word) => lower === word || lower.startsWith(`${word} `));
}

export function textBook(text, fallbackTitle) {
  let lines = text.replaceAll("\r\n", "\n").replaceAll("\r", "\n").split("\n");
  const gutenberg = gutenbergParts(lines);
  if (gutenberg) lines = lines.slice(gutenberg.start + 1, gutenberg.end);

  // Wrapped by hand when a blank line stands between two lines of text.
  const first = lines.findIndex((line) => !isBlank(line));
  const last = lines.findLastIndex((line) => !isBlank(line));
  const wrapped = lines.slice(first, last + 1).some(isBlank);

  const paragraphs = [];
  let run = [];
  const end = () => {
    if (!run.length) return;
    const paragraph = collapsed(run.join(" "));
    const opens = opensChapter(paragraph);
    paragraphs.push({ text: paragraph, heading: opens, opens });
    run = [];
  };
  for (const line of lines) {
    if (isBlank(line)) {
      end();
    } else {
      run.push(line);
      if (!wrapped) end();
    }
  }
  end();

  const title = gutenberg?.title || fallbackTitle;
  return { title, author: gutenberg?.author ?? "", language: "", chapters: textChapters(paragraphs, title) };
}
