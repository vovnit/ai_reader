// A photo read more than once by Tesseract, put together. Its page
// layout keeps columns in order, but takes print that a finger or a
// picture touches for part of a picture and leaves it out; read again as
// scattered text, without a layout, the photo gives back that print, and
// read small it gives the titles too large for Tesseract to read at full
// size. Lines are `tesseractLines`' (see `PointedWord`).

/** `line` read from a copy of the photo `scale` times the size of the full one, in the full one's pixels. */
export function scaledLine(line, scale) {
  const k = 1 / scale;
  return {
    ...line,
    height: line.height * k,
    words: line.words.map((word) => ({ ...word, box: { x: word.box.x * k, y: word.box.y * k, width: word.box.width * k, height: word.box.height * k } })),
  };
}

/**
 * The layout's `lines` with the `more` lines another reading found where
 * they have none, each put in reading order: after the nearest line above
 * it in its column, and after whatever follows that line until a line
 * comes after it in the page or a new column begins.
 */
export function mergedLines(lines, more) {
  const merged = [...lines];
  // One page's axes for every line: a short line's own baseline can slant
  // enough to move it a row at the far side of the photo.
  const sum = lines.reduce((total, line) => ({ dx: total.dx + line.up.dx, dy: total.dy + line.up.dy }), { dx: 0, dy: 0 });
  const length = Math.hypot(sum.dx, sum.dy);
  const up = length ? { dx: sum.dx / length, dy: sum.dy / length } : { dx: 0, dy: -1 };
  const bands = merged.map((line) => bandOf(line, up));
  const added = more.map((line) => ({ line, band: bandOf(line, up) })).sort((a, b) => a.band.middle - b.band.middle);
  for (const { line, band } of added) {
    // Read without a layout, a picture gives stray letters and marks.
    if (!/\p{L}{3}/u.test(line.text)) continue;
    if (bands.some((other) => sameRow(other, band) && other.words.some((word) => band.words.some((own) => meet(word, own))))) continue;
    const at = placeOf(band, bands);
    merged.splice(at, 0, line);
    bands.splice(at, 0, band);
  }
  return merged;
}

function placeOf(band, bands) {
  let after = -1;
  bands.forEach((other, index) => {
    if (sameRow(other, band) || other.middle > band.middle || !meet(other, band)) return;
    if (after < 0 || other.middle > bands[after].middle) after = index;
  });
  // Nothing above it: it follows what is left of it in its row.
  if (after < 0) after = bands.findLastIndex((other) => sameRow(other, band) && other.start < band.start);
  if (after < 0) {
    const below = bands.findIndex((other) => !sameRow(other, band) && other.middle > band.middle && meet(other, band));
    return below < 0 ? bands.length : below;
  }
  const top = bands[after];
  let at = after + 1;
  for (; at < bands.length; at++) {
    const next = bands[at];
    // Back up the page is the next column.
    if (!sameRow(next, top) && next.middle < top.middle) break;
    if (sameRow(next, band) ? next.start > band.start : next.middle > band.middle) break;
  }
  return at;
}

/**
 * Where a line lies along the axes of a page whose top faces `up`, however
 * it is turned: `start` and `end` along its lines, `middle` down the page,
 * and the same span of each word. The middle is its words' median, as a
 * word's box can reach into the next line.
 */
function bandOf(line, up) {
  const along = { dx: -up.dy, dy: up.dx };
  const down = { dx: -up.dx, dy: -up.dy };
  const words = line.words.map(({ box }) => {
    const x = box.x + box.width / 2, y = box.y + box.height / 2;
    const middle = x * along.dx + y * along.dy;
    const half = (Math.abs(along.dx) * box.width + Math.abs(along.dy) * box.height) / 2;
    return { start: middle - half, end: middle + half, middle: x * down.dx + y * down.dy };
  });
  const middles = words.map((word) => word.middle).sort((a, b) => a - b);
  return {
    words,
    start: Math.min(...words.map((word) => word.start)),
    end: Math.max(...words.map((word) => word.end)),
    middle: middles[Math.floor(middles.length / 2)],
    height: line.height,
  };
}

function sameRow(a, b) {
  return Math.abs(a.middle - b.middle) < Math.max(a.height, b.height) / 2;
}

/** Whether two spans along the lines overlap. */
function meet(a, b) {
  return a.start < b.end && b.start < a.end;
}
