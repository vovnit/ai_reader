// Finds the word a finger points at on a photographed page. A finger is
// laid just short of the word it means, usually below it, so the word is
// the one nearest ahead of the tip, along the way the finger points;
// whatever is behind the tip is under the finger. The same as the iOS
// app's `PointedWord`.
//
// A photo is `{ lines, fingers }`, positions in its pixels from its top
// left corner, the photo turned upright. A line is
// `{ text, words, height, up }`: how tall it is and which way its top
// faces, one pixel long, are the true ones, from its corners, however the
// page is turned in the photo; its words' boxes only bound them. A word is
// `{ location, length, box }`, where it is in its line's text in UTF-16
// units, and `box` `{ x, y, width, height }`. A finger is
// `{ tip: { x, y }, direction: { dx, dy } }`: the tip of an index finger
// and the way it points, from the joint below the tip toward it, one pixel
// long.

/**
 * `{ line, word }`, indices into the photo's lines and that line's words;
 * null when no finger was seen, or none points near the text — a hand
 * holding the book rather than pointing into it.
 */
export function pointedWord(photo) {
  let best = null;
  for (const finger of photo.fingers) {
    const found = aim(finger, photo);
    if (found && (!best || found.score < best.score)) best = found;
  }
  return best?.place ?? null;
}

/**
 * `{ place: { line, word }, score }`: the word one finger points at, and
 * how far the finger is from pointing straight at it, in pixels; null when
 * it points at none.
 */
export function aim(finger, photo) {
  const heights = photo.lines.flatMap((line) => line.words.map((word) => word.box.height)).sort((a, b) => a - b);
  if (!heights.length) return null;
  const lineHeight = heights[Math.floor(heights.length / 2)];

  let best = null;
  photo.lines.forEach((line, lineIndex) => {
    line.words.forEach((word, wordIndex) => {
      const found = score(word.box, finger, lineHeight);
      if (found !== null && found < (best?.score ?? Infinity)) best = { place: { line: lineIndex, word: wordIndex }, score: found };
    });
  });
  return best;
}

/**
 * How far the box is from the fingertip, counting a step aside from the
 * finger's line against it as much as a step along it. Null for a box
 * behind the tip or out of its reach.
 */
function score(box, finger, lineHeight) {
  const { tip, direction } = finger;
  const minX = box.x, maxX = box.x + box.width, minY = box.y, maxY = box.y + box.height;
  // Halfway between the edges, as Foundation's `CGRect.midX` is, to the bit.
  const midX = (minX + maxX) / 2, midY = (minY + maxY) / 2;
  // The tip may rest on the lower part of the word it means.
  const ahead = (midX - tip.x) * direction.dx + (midY - tip.y) * direction.dy;
  if (!(ahead > -lineHeight / 4)) return null;

  const dx = Math.min(Math.max(tip.x, minX), maxX) - tip.x;
  const dy = Math.min(Math.max(tip.y, minY), maxY) - tip.y;
  const distance = Math.sqrt(dx * dx + dy * dy);
  if (!(distance < lineHeight * 3)) return null;
  const aside = Math.abs(dx * direction.dy - dy * direction.dx);
  return distance + aside;
}
