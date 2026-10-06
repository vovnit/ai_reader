// Turns a tap's offset into the word that was touched and the sentence it
// sits in, by the platform's own word and sentence boundaries — what Pango
// gives the Kindle app and NaturalLanguage the iOS app. A sentence never
// runs past its paragraph.

function segmenter(language, granularity) {
  try {
    return new Intl.Segmenter(language || undefined, { granularity });
  } catch {
    return new Intl.Segmenter(undefined, { granularity });
  }
}

/**
 * `{ word, sentence, start, end, sentenceStart, sentenceEnd }` for the word
 * at `offset`, or null when the tap was not on one.
 */
export function selectionAt(text, offset, language) {
  if (offset < 0 || offset >= text.length || text[offset] === "\n") return null;
  const paragraphStart = text.lastIndexOf("\n", offset - 1) + 1;
  let paragraphEnd = text.indexOf("\n", offset);
  if (paragraphEnd < 0) paragraphEnd = text.length;
  const paragraph = text.slice(paragraphStart, paragraphEnd);

  const word = segmenter(language, "word").segment(paragraph).containing(offset - paragraphStart);
  if (!word || !word.isWordLike) return null;
  const sentence = segmenter(language, "sentence").segment(paragraph).containing(word.index) ?? word;
  return {
    word: word.segment,
    sentence: sentence.segment.trim(),
    start: paragraphStart + word.index,
    end: paragraphStart + word.index + word.segment.length,
    sentenceStart: paragraphStart + sentence.index,
    sentenceEnd: paragraphStart + sentence.index + sentence.segment.length,
  };
}
