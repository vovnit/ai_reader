// Reads a plain-text book, already decoded, into `{ title, author, language,
// chapters }`. Text wrapped by hand, with blank lines between paragraphs,
// is joined back into paragraphs; text without blank lines has one
// paragraph per line. A short paragraph that is a chapter's number, or
// names a chapter the way a heading does, opens a chapter. The same as
// `TextBook.cpp` (Kindle) and `TextBook.swift` (iOS).
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

/**
 * How many letters of `text`, from `i`, one place of a roman numeral takes
 * when written the standard way: up to three `one`s, `one` and `five`,
 * `five` and up to three `one`s, or `one` and `ten`.
 */
function romanPlace(text, i, one, five, ten) {
  if (text[i] === one && (text[i + 1] === five || text[i + 1] === ten)) return 2;
  const start = text[i] === five ? 1 : 0;
  let ones = 0;
  while (ones < 3 && text[i + start + ones] === one) ones++;
  return start + ones;
}

/** "I" to "MMMCMXCIX", written the standard way: "DID" and "CIVIL" are words, not numbers. */
function isRoman(text) {
  let i = 0;
  while (i < 3 && text[i] === "M") i++;
  i += romanPlace(text, i, "C", "D", "M");
  i += romanPlace(text, i, "X", "L", "C");
  i += romanPlace(text, i, "I", "V", "X");
  return text !== "" && i === text.length;
}

/** A chapter's number: up to three digits, or an upper-case roman numeral. */
const isNumber = (text) => /^[0-9]{1,3}$/.test(text) || isRoman(text);

/** "I.", "12:", "IV—The Return": a number, then nothing or a stop, colon or dash and anything. */
const startsWithNumber = (word) => isNumber(word.split(/[.:\u2014-]/)[0]);

/**
 * "XIV", "12.", "CHAPTER I.", "Глава 3", "Prologue", "BOOK ONE: 1805": a
 * number alone, or a chapter word alone, before a number, or in a line
 * without a lower-case letter. "Part of me wanted to stay." is a sentence.
 * Kept simple, since a heading one app read differently would move every
 * chapter after it.
 */
export function opensChapter(text) {
  if ([...text].length > headingLength) return false;
  if (isNumber(text.endsWith(".") ? text.slice(0, -1) : text)) return true;
  // A no-break space, which French sets before ":", parts words too.
  const words = text.split(/[ \u00a0]/).filter((word) => word);
  if (!words.length) return false;
  const first = words[0].toLowerCase();
  if (words.length === 1) return chapterWords.some((word) => first === word || first === `${word}.` || first === `${word}:`);
  return chapterWords.includes(first) && (startsWithNumber(words[1]) || !/\p{Ll}/u.test(text));
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
