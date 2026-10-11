// A photo's lines run together as the page reads, so the sentence around a
// word can be found across line breaks, and where in it the word the finger
// points at starts. The same as the iOS app's `PhotoPassage`.
import { joined } from "../Books/PdfParagraphs.js";
import { pointedWord } from "./PointedWord.js";

/** `{ text, pointedOffset }`: the offset is in UTF-16 units into `text`, null when no finger pointed at a word. */
export function photoPassage(photo) {
  let text = "";
  const lineStarts = [];
  for (const line of photo.lines) {
    // Joined the way a PDF's lines are, a word hyphenated at a line's end mended.
    text = text === "" ? line.text : joined(text, line.text);
    lineStarts.push(text.length - line.text.length);
  }
  const place = pointedWord(photo);
  const pointedOffset = place ? lineStarts[place.line] + photo.lines[place.line].words[place.word].location : null;
  return { text, pointedOffset };
}
