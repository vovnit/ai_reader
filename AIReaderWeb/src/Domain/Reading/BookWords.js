// Every distinct word form in a book, in reading order, with the first
// places it appears: what its glossary is written from. Words are the ones
// a tap finds (WordContext), normalized as the dictionaries look them up.
// Only the first places, so a definition of a name cannot give away what
// happens later.
import { normalizeWord } from "../Dictionary/WordNormalizer.js";

const examplesPerForm = 3;
/** Words of context on each side of an example. */
const contextWords = 8;

function wordSegmenter(language) {
  try {
    return new Intl.Segmenter(language || undefined, { granularity: "word" });
  } catch {
    return new Intl.Segmenter(undefined, { granularity: "word" });
  }
}

/** Letters, joined by apostrophes or hyphens: numbers have nothing to define. */
const isDefinable = (word) => /^[\p{L}\p{M}'’ʼ-]+$/u.test(word) && /\p{L}/u.test(word);

/** `[{ form, spelling, examples }]` from the chapters' texts, paragraphs a line each. */
export function bookWords(chapters, language) {
  const segmenter = wordSegmenter(language);
  const words = new Map();
  for (const chapter of chapters) {
    for (const paragraph of chapter.split("\n")) {
      const found = [...segmenter.segment(paragraph)].filter((segment) => segment.isWordLike);
      found.forEach((segment, index) => {
        if (!isDefinable(segment.segment)) return;
        const form = normalizeWord(segment.segment);
        if (!form) return;
        if (!words.has(form)) words.set(form, { form, spelling: segment.segment, examples: [] });
        const word = words.get(form);
        if (word.examples.length >= examplesPerForm) return;
        const example = exampleAt(paragraph, found, index);
        if (!word.examples.includes(example)) word.examples.push(example);
      });
    }
  }
  return [...words.values()];
}

function exampleAt(paragraph, found, index) {
  const start = index <= contextWords ? 0 : found[index - contextWords].index;
  const last = index + contextWords;
  const end = last >= found.length - 1 ? paragraph.length : found[last].index + found[last].segment.length;
  return `${start ? "…" : ""}${paragraph.slice(start, end).trim()}${end < paragraph.length ? "…" : ""}`;
}
