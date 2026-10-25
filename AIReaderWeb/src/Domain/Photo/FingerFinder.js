// Finds a pointing finger in a photo's pixels: a single finger that comes
// into the photo from its edge, without the palm and knuckles a hand pose
// needs. It is a smooth, skin-coloured band that ends, in a round cap, just
// below a word. Null rather than a guess, since a wrong finger would
// explain a word the reader never pointed at. The same as the iOS app's
// `FingerFinder`. Masks are `Uint8Array`s, 1 for a pixel in them.
import { fingerShape } from "./FingerShape.js";
import { aim } from "./PointedWord.js";
import { cover, skinMask } from "./SkinMask.js";

/**
 * The finger, `{ tip, direction }` in the full photo's pixels, or null.
 * `image` is the photo shrunk, an `RgbImage` with its long side about 256
 * pixels — enough to keep a finger tens of pixels wide — and `lines` its
 * text in the full photo's pixels; `photoSize` is `{ width, height }`.
 */
export function findFinger(image, lines, photoSize) {
  const scale = image.width / photoSize.width;
  const boxes = lines.flatMap((line) => line.words).map(({ box }) => ({
    x: box.x * scale, y: box.y * scale, width: box.width * scale, height: box.height * scale,
  }));
  const heights = lines.map((line) => line.height).sort((a, b) => a - b);
  const mask = heights.length ? skinMask(image, boxes) : null;
  if (!mask) return null;
  const lineHeight = heights[Math.floor(heights.length / 2)];
  const up = lines.reduce((sum, line) => ({ dx: sum.dx + line.up.dx, dy: sum.dy + line.up.dy }), { dx: 0, dy: 0 });
  const length = Math.max(Math.sqrt(up.dx * up.dx + up.dy * up.dy), 1e-6);
  const text = { lines, fingers: [] };

  // A light finger first, then a dark one, taking in dimmer pixels but
  // asking for a closer likeness, since more of a dark background can join
  // it then. The floors are 32-bit, as the brightness they are held to.
  for (const [floor, fit] of [[Math.fround(0.3), 0.65], [Math.fround(0.15), 0.8]]) {
    let best = null;
    // A finger is wider than two lines of text and longer than it is wide.
    const area = Math.ceil((2 * lineHeight * scale) * (2 * lineHeight * scale));
    for (const region of candidates(mask, floor, area)) {
      const shape = fingerShape(region, mask.width, mask.height, {
        fit, lineHeight: lineHeight * scale, up: { dx: up.dx / length, dy: up.dy / length },
      });
      if (!shape || surroundsPrint(region, boxes, mask.width, mask.height, area)) continue;
      const finger = {
        tip: { x: (shape.tip.x + 0.5) / scale, y: (shape.tip.y + 0.5) / scale },
        direction: shape.direction,
      };
      // A finger pointing at a word rests within a line and a half of it.
      const pointed = aim(finger, text);
      const score = pointed?.score;
      if (score === undefined || !(score <= 1.5 * lineHeight) || !(score < (best?.score ?? Infinity))) continue;
      // Print on a skin-coloured page joins the page to the finger, and
      // the page's own edge can pass for one: a finger is told from the
      // paper by the word it points at, printed on something else.
      const { box } = lines[pointed.place.line].words[pointed.place.word];
      if (onSkin(mask, { x: box.x * scale, y: box.y * scale, width: box.width * scale, height: box.height * scale })) continue;
      best = { finger, score };
    }
    if (best) return best.finger;
  }
  return null;
}

/**
 * Whether the region closes around a patch of print wider than a finger,
 * as a skin-coloured page does around a white box and a finger never does;
 * paper in a hand's shadow, skin-coloured too, closes only around a word's
 * ink. `boxes` are the words', in the shrunk photo's pixels, and
 * `fingerArea` the least a finger covers.
 */
function surroundsPrint(region, boxes, width, height, fingerArea) {
  const count = width * height;
  const inRegion = new Uint8Array(count);
  for (const i of region) inRegion[i] = 1;
  const outsideRegion = inRegion.map((value) => 1 - value);
  // What can be reached from the photo's edge without crossing the region.
  const edge = new Uint8Array(count);
  for (let i = 0; i < count; i++) {
    const x = i % width, y = (i - x) / width;
    edge[i] = x === 0 || y === 0 || x === width - 1 || y === height - 1 ? 1 : 0;
  }
  const outside = flood(outsideRegion, edge, width, height);
  const hole = outsideRegion.map((value, i) => value & (1 - outside[i]));
  return boxes.some((box) => {
    // Halfway between the edges, as Foundation's `CGRect.midX` is.
    const x = Math.min(width - 1, Math.max(0, Math.trunc((box.x + box.x + box.width) / 2)));
    const y = Math.min(height - 1, Math.max(0, Math.trunc((box.y + box.y + box.height) / 2)));
    if (!hole[y * width + x]) return false;
    const seed = new Uint8Array(count);
    seed[y * width + x] = 1;
    return flood(hole, seed, width, height).reduce((sum, value) => sum + value, 0) > 4 * fingerArea;
  });
}

/** Whether most of `box`, in the shrunk photo's pixels, is skin-coloured. */
function onSkin(mask, box) {
  const covered = cover([box], mask.width, mask.height);
  let inside = 0, skin = 0;
  for (let i = 0; i < covered.length; i++) {
    if (!covered[i]) continue;
    inside++;
    skin += mask.skin[i];
  }
  return skin > inside / 2;
}

/**
 * The smooth skin-coloured regions, each cut from its neighbours where a
 * weak edge joins them, then grown back over the outline, the nail and the
 * folds the cut and the smoothness took; and each one's whole
 * skin-coloured area too, since on print with little texture to judge by,
 * smoothness keeps only part of a finger lit from the side. Each region is
 * its pixels' indices, in order.
 */
function candidates(mask, floor, minimumArea) {
  const { width, height } = mask;
  const count = width * height;
  const skin = new Uint8Array(count);
  let core = new Uint8Array(count);
  for (let i = 0; i < count; i++) {
    skin[i] = mask.skin[i] && mask.brightness[i] > floor ? 1 : 0;
    core[i] = mask.smooth[i] & skin[i];
  }
  for (let round = 0; round < 2; round++) core = step(core, width, height, false);

  const seen = new Uint8Array(count);
  const stack = new Int32Array(count);
  const members = new Int32Array(count);
  let top = 0;
  const reach = (j) => {
    if (core[j] && !seen[j]) {
      seen[j] = 1;
      stack[top++] = j;
    }
  };
  const found = [];
  for (let start = 0; start < count; start++) {
    if (!core[start] || seen[start]) continue;
    let area = 0;
    stack[top++] = start;
    seen[start] = 1;
    while (top) {
      const i = stack[--top];
      members[area++] = i;
      const x = i % width, y = (i - x) / width;
      if (x + 1 < width) reach(i + 1);
      if (x > 0) reach(i - 1);
      if (y + 1 < height) reach(i + width);
      if (y > 0) reach(i - width);
    }
    if (area < minimumArea) continue;
    const region = new Uint8Array(count);
    for (let k = 0; k < area; k++) region[members[k]] = 1;
    let grown = region;
    for (let round = 0; round < 3; round++) {
      const wider = step(grown, width, height, true);
      for (let i = 0; i < count; i++) wider[i] = (wider[i] & skin[i]) | grown[i];
      grown = wider;
    }
    const whole = flood(skin, region, width, height);
    found.push(indices(grown));
    if (!grown.every((value, i) => value === whole[i])) found.push(indices(whole));
  }
  return found;
}

/** The part of `mask` joined to `seed` through its four neighbours. */
export function flood(mask, seed, width, height) {
  const count = width * height;
  const out = new Uint8Array(count);
  const stack = new Int32Array(count);
  let top = 0;
  const reach = (j) => {
    if (mask[j] && !out[j]) {
      out[j] = 1;
      stack[top++] = j;
    }
  };
  for (let i = 0; i < count; i++) if (seed[i]) reach(i);
  while (top) {
    const i = stack[--top];
    const x = i % width, y = (i - x) / width;
    if (x + 1 < width) reach(i + 1);
    if (x > 0) reach(i - 1);
    if (y + 1 < height) reach(i + width);
    if (y > 0) reach(i - width);
  }
  return out;
}

/** The mask grown or shrunk by a pixel toward its four neighbours. */
function step(mask, width, height, grow) {
  const out = new Uint8Array(mask.length);
  // The photo's edge neither grows nor wears the mask away.
  const edge = grow ? 0 : 1;
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const i = y * width + x;
      const left = x > 0 ? mask[i - 1] : edge, right = x < width - 1 ? mask[i + 1] : edge;
      const above = y > 0 ? mask[i - width] : edge, below = y < height - 1 ? mask[i + width] : edge;
      out[i] = grow ? mask[i] | left | right | above | below : mask[i] & left & right & above & below;
    }
  }
  return out;
}

function indices(mask) {
  const found = [];
  for (let i = 0; i < mask.length; i++) if (mask[i]) found.push(i);
  return Int32Array.from(found);
}
