// A photo's lines run together as the page reads, so the sentence around a
// word can be found across line breaks, and where in it the word the finger
// points at starts — or, with no finger, each word read off the photo, to
// tap one where it is printed. The same as the iOS app's `PhotoPassage`.
import { joined } from "../Books/PdfParagraphs.js";
import { photoWords } from "./PhotoWord.js";
import { pointedWord } from "./PointedWord.js";

/**
 * `{ text, pointedOffset, words, photoSize }`: the offset is in UTF-16 units
 * into `text`, null when no finger pointed at a word; `words` are
 * `photoWords`, placed in the photo's pixels, `photoSize` its size.
 */
export function photoPassage(photo) {
  let text = "";
  const lineStarts = [];
  for (const line of photo.lines) {
    // Joined the way a PDF's lines are, a word hyphenated at a line's end mended.
    text = text === "" ? line.text : joined(text, line.text);
    lineStarts.push(text.length - line.text.length);
  }
  const lineWords = photo.lines.map((line, index) => photoWords(line, lineStarts[index]));
  const place = pointedWord(photo);
  const pointedOffset = place ? lineWords[place.line][place.word].offset : null;
  return { text, pointedOffset, words: lineWords.flat(), photoSize: photo.size };
}
