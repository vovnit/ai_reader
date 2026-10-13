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
  const across = line.words.map(({ box }) => (box.x + box.width / 2) * up.dx + (box.y + box.height / 2) * up.dy);
  const print = printBand(line, across, cos, sin);
  return line.words.map((word, index) => {
    const { box } = word;
    const text = line.text.slice(word.location, word.location + word.length);
    // A word read with a quote or a dash before it starts at its first
    // letter or digit, where a tap finds it.
    const lead = Math.max(text.search(/[\p{L}\p{N}]/u), 0);
    const height = boxHeight(box, cos, sin) ?? print.height;
    // The box stands upright around the word turned with the page, its
    // sides the word's length and its height crossed.
    const length = cos >= sin ? (box.width - height * sin) / cos : (box.height - height * cos) / sin;
    // Every word sits on the line's print, as a box reaching up to an
    // ascender or down to a descender does not.
    const shift = print.middle - across[index];
    return {
      text,
      offset: lineStart + word.location + lead,
      center: { x: box.x + box.width / 2 + shift * up.dx, y: box.y + box.height / 2 + shift * up.dy },
      // At least a letter wide, which a slanted box can come short of.
      size: { width: Math.max(length, print.height / 2), height: print.height },
      angle: Math.atan2(along.dy, along.dx),
    };
  });
}

/**
 * Where the line's print lies across it, `{ middle, height }` along its
 * `up`: from the top of its tallest letter to the foot of its lowest. Near
 * half a right angle a box's sides cannot be told apart, and the line's own
 * height is taken, about the middle of its words.
 */
function printBand(line, across, cos, sin) {
  const heights = line.words.map(({ box }) => boxHeight(box, cos, sin));
  if (!heights.length || heights[0] === null) {
    const sorted = [...across].sort((a, b) => a - b);
    return { middle: sorted.length ? sorted[Math.floor(sorted.length / 2)] : 0, height: line.height };
  }
  const top = Math.max(...across.map((middle, index) => middle + heights[index] / 2));
  const foot = Math.min(...across.map((middle, index) => middle - heights[index] / 2));
  return { middle: (top + foot) / 2, height: Math.max(top - foot, 1) };
}

/**
 * How tall the word in `box` is. Once the page is turned, an upright box
 * mixes the word's length into its height; its two sides give both back,
 * except near half a right angle, where they are alike: then null.
 */
function boxHeight(box, cos, sin) {
  const turn = cos * cos - sin * sin;
  return Math.abs(turn) < 0.2 ? null : Math.max((box.height * cos - box.width * sin) / turn, 0);
}
