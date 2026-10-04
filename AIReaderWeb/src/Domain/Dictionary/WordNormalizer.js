// Puts a word into the shape the dictionary's `normalized_form` column
// uses: composed, typographic apostrophes and dashes made plain, lowercased,
// and trimmed of punctuation, symbols and spaces at both ends.
import { isPunctuationOrSymbol, isSpace } from "../../Support/Text.js";

const strippable = (character) => isSpace(character) || isPunctuationOrSymbol(character);

export function normalizeWord(word) {
  const characters = [
    ...word
      .normalize("NFC")
      .replace(/[’ʼ＇]/g, "'")
      .replace(/[‐‑‒–—]/g, "-")
      .toLowerCase(),
  ];
  let start = 0;
  let end = characters.length;
  while (start < end && strippable(characters[start])) start++;
  while (end > start && strippable(characters[end - 1])) end--;
  return characters.slice(start, end).join("");
}
