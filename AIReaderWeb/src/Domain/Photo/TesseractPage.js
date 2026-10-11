// Tesseract.js's reading of a photographed page, as the lines a photo is
// made of (see `PointedWord`): each line's text, where each of its words
// sits, and how tall the line is and which way its top faces. What the iOS
// app's `PhotoReaderClient` makes of Vision's lines.
import { median } from "./SkinMask.js";

// Below this Tesseract is guessing, at a finger's edge or a picture, and a
// word it invents could be taken for the one pointed at.
const leastConfidence = 30;

/**
 * The lines of `blocks`, `recognize(image, {}, { blocks: true }).data.blocks`,
 * in the pixels of the image that was read. Words Tesseract is unsure of are
 * left out, and a line left with none.
 */
export function tesseractLines(blocks) {
  const lines = [];
  for (const block of blocks) {
    for (const paragraph of block.paragraphs) {
      for (const found of paragraph.lines) {
        const line = photoLine(found);
        if (line) lines.push(line);
      }
    }
  }
  return lines;
}

function photoLine(found) {
  let text = "";
  const words = [];
  for (const word of found.words) {
    // A word's text can begin with a space Tesseract read as a letter.
    const spelling = word.text.trim();
    if (!spelling || word.confidence < leastConfidence) continue;
    if (text) text += " ";
    words.push({ location: text.length, length: spelling.length, box: box(word.bbox) });
    text += spelling;
  }
  if (!words.length) return null;
  // As tall as its words' boxes: the finger finder was tuned on lines
  // measured so.
  const height = Math.max(median(words.map((word) => word.box.height)), 1);
  return { text, words, height, up: up(found.baseline) };
}

/**
 * Which way the line's top faces, one pixel long: across its baseline,
 * which follows the line however the page is turned in the photo.
 */
function up(baseline) {
  const dx = baseline ? baseline.x1 - baseline.x0 : 0;
  const dy = baseline ? baseline.y1 - baseline.y0 : 0;
  const length = Math.sqrt(dx * dx + dy * dy);
  if (!length) return { dx: 0, dy: -1 };
  // A quarter turn back from the way the line reads, the photo's y running down.
  return { dx: dy / length, dy: -dx / length };
}

function box({ x0, y0, x1, y1 }) {
  return { x: x0, y: y0, width: x1 - x0, height: y1 - y0 };
}
