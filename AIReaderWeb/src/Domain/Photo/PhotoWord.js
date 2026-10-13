// A word read off a photo, placed to lay its text over the print it was
// read from, and where it starts in the photo's passage. Places are in the
// photo's pixels. The same as the iOS app's `PhotoWord`.

/**
 * The words of `line`, which starts at `lineStart` in the passage: each
 * `{ text, offset, center, size, angle }`, its text as the line has it (a
 * hyphen at the line's end kept), its size along the line and across it,
 * and the way the line reads in radians clockwise from the photo's x axis.
 */
export function photoWords(line, lineStart) {
  const { up } = line;
  const along = { dx: -up.dy, dy: up.dx };
  const cos = Math.abs(along.dx);
  const sin = Math.abs(along.dy);
  const acrossOf = ({ box }) => (box.x + box.width / 2) * up.dx + (box.y + box.height / 2) * up.dy;
  // Each word is centred across where most of the line's are, as a box
  // reaching up to an ascender or down to a descender is not.
  const across = line.words.map(acrossOf).sort((a, b) => a - b);
  const middle = across.length ? across[Math.floor(across.length / 2)] : 0;
  return line.words.map((word) => {
    const { box } = word;
    // The box stands upright around the word turned with the page, its
    // sides the word's length and the line's height crossed.
    const length = cos >= sin ? (box.width - line.height * sin) / cos : (box.height - line.height * cos) / sin;
    const shift = middle - acrossOf(word);
    return {
      text: line.text.slice(word.location, word.location + word.length),
      offset: lineStart + word.location,
      center: { x: box.x + box.width / 2 + shift * up.dx, y: box.y + box.height / 2 + shift * up.dy },
      // At least a letter wide, which a slanted box can come short of.
      size: { width: Math.max(length, line.height / 2), height: line.height },
      angle: Math.atan2(along.dy, along.dx),
    };
  });
}
