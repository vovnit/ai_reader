// Whether a region of a shrunk photo is shaped like a pointing finger, and
// where its tip is: a band at least two lines wide that comes in from the
// photo's edge, up the page or across it, and ends in a round cap inside
// it. The same as the iOS app's `FingerShape`. Points are two arrays, `xs`
// and `ys`, in the order of their pixels' indices, which sums follow.
import { flood } from "./FingerFinder.js";
import { median } from "./SkinMask.js";

/**
 * `{ tip: { x, y }, direction: { dx, dy } }`, or null for a region shaped
 * otherwise. The direction is the way to the word: halfway between the
 * finger's own axis and the page's up, since readers hold the finger under
 * the word. `pixels` are the region's, as indices `y * width + x` into a
 * photo `width` by `height`; `fit` is how closely the end must match an
 * ideal finger, from 0 to 1; `up` is the page's, one pixel long.
 */
export function fingerShape(pixels, width, height, { fit, lineHeight, up }) {
  const entry = [];
  for (const i of pixels) {
    const x = i % width, y = (i - x) / width;
    if (x < 2 || y < 2 || x >= width - 2 || y >= height - 2) entry.push(i);
  }
  if (!entry.length) return null;
  const door = mean(...coordinates(entry, width));
  const end = endOf(pixels, entry, width, height, 12 * lineHeight);
  if (end.length < 3) return null;
  const [xs, ys] = coordinates(end, width);

  // From where it enters toward its end's middle, then along the far end
  // alone, not the hand behind it.
  const centre = mean(xs, ys);
  let axis = unit(centre.x - door.x, centre.y - door.y);
  const across = Math.sqrt(12 * Math.max(0, spread(xs, ys).least));
  for (let round = 0; round < 2; round++) {
    const major = spread(...nearer(xs, ys, depthsOf(xs, ys, axis).depths, 2 * across)).major;
    axis = major.x * axis.x + major.y * axis.y > 0 ? major : { x: -major.x, y: -major.y };
  }
  // Up the page or across it, at most 30 degrees down; what hangs further
  // down is a picture or the room, not a pointing finger.
  if (!(axis.x * up.dx + axis.y * up.dy >= -0.5)) return null;

  const { front, depths } = depthsOf(xs, ys, axis);
  const slices = slicesOf(depths);
  const wide = widthOf(slices, across);
  if (wide === null || !(wide >= 2 * lineHeight)
    || !(fitOf(xs, ys, depths, axis, front, wide, width, height) >= fit)
    // An eighth of a width behind the tip a fingertip measures under nine
    // tenths of its width; a box or a band as wide.
    || !(slices[Math.trunc(wide / 8)] <= 0.92 * wide)) return null;

  const lead = unit(axis.x + up.dx, axis.y + up.dy);
  const [capX, capY] = nearer(xs, ys, depths, wide / 2);
  const reach = along(capX, capY, lead);
  const farthest = largest(reach);
  const foremost = mean(capX.filter((_, k) => reach[k] >= farthest - 1), capY.filter((_, k) => reach[k] >= farthest - 1));
  // Where the region leaves the photo no tip is in sight.
  if (!(foremost.x >= 1 && foremost.x < width - 2 && foremost.y >= 1 && foremost.y < height - 2)) return null;
  return { tip: { x: foremost.x + lead.x, y: foremost.y + lead.y }, direction: { dx: lead.x, dy: lead.y } };
}

/**
 * The finger's end: what lies within `reach` of the point farthest from the
 * photo's edge through the region, and joins it there. That point is a
 * thumb's tip, not the top of the hand holding the book beside it, which
 * runs off the edge itself; `reach`, twelve lines, is more than a width and
 * a third of the widest finger. Pixel indices, in order.
 */
function endOf(pixels, entry, width, height, reach) {
  const count = width * height;
  const steps = new Int32Array(count).fill(-1);
  const inRegion = new Uint8Array(count);
  for (const i of pixels) inRegion[i] = 1;
  const queue = new Int32Array(pixels.length);
  queue.set(entry);
  let tail = entry.length;
  for (const i of entry) steps[i] = 0;
  const visit = (j, reached) => {
    if (inRegion[j] && steps[j] < 0) {
      steps[j] = reached;
      queue[tail++] = j;
    }
  };
  for (let head = 0; head < tail; head++) {
    const i = queue[head];
    const x = i % width, y = (i - x) / width;
    if (x + 1 < width) visit(i + 1, steps[i] + 1);
    if (x > 0) visit(i - 1, steps[i] + 1);
    if (y + 1 < height) visit(i + width, steps[i] + 1);
    if (y > 0) visit(i - width, steps[i] + 1);
  }
  let farthest = 0;
  for (let k = 0; k < tail; k++) farthest = Math.max(farthest, steps[queue[k]]);
  const ends = [];
  for (let k = 0; k < tail; k++) if (steps[queue[k]] === farthest) ends.push(queue[k]);
  const far = mean(...coordinates(ends, width));

  let near = new Uint8Array(count);
  for (const i of pixels) {
    const x = i % width, y = (i - x) / width;
    if (Math.sqrt((x - far.x) * (x - far.x) + (y - far.y) * (y - far.y)) <= reach) near[i] = 1;
  }
  const start = roundedHalfEven(far.y) * width + roundedHalfEven(far.x);
  if (near[start]) {
    const seed = new Uint8Array(count);
    seed[start] = 1;
    near = flood(near, seed, width, height);
  }
  const found = [];
  for (let i = 0; i < count; i++) if (near[i]) found.push(i);
  return found;
}

/** The finger's width: the typical slice of its far end, measured again from that first guess. */
function widthOf(slices, start) {
  let wide = start;
  for (let round = 0; round < 2; round++) {
    // `subarray` stops at the deepest slice, as the iOS app's `min`s do.
    const counts = slices.subarray(Math.trunc(wide / 2), Math.trunc(2 * wide)).filter((count) => count > 0);
    if (!counts.length) return null;
    wide = median(counts);
  }
  return wide;
}

/**
 * How much the far end overlaps an ideal finger, a band its width that
 * ends in a half disc, over its first two widths within the photo.
 */
function fitOf(xs, ys, depths, axis, front, wide, width, height) {
  const radius = wide / 2;
  const length = Math.min(2 * wide, largest(depths) + 1);
  // Too little shows to tell a finger from any round blob.
  if (!(length >= wide)) return 0;
  const sides = [];
  for (let k = 0; k < xs.length; k++) {
    if (depths[k] < length && depths[k] >= radius) sides.push(-xs[k] * axis.y + ys[k] * axis.x);
  }
  if (!sides.length) return 0;
  const middle = median(sides);
  const region = new Uint8Array(width * height);
  let regionCount = 0;
  for (let k = 0; k < xs.length; k++) {
    const i = Math.trunc(ys[k]) * width + Math.trunc(xs[k]);
    if (depths[k] < length && !region[i]) {
      region[i] = 1;
      regionCount++;
    }
  }
  let ideal = 0, both = 0;
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const depth = front - (x * axis.x + y * axis.y);
      if (!(depth >= 0 && depth < length)) continue;
      const half = depth < radius ? Math.sqrt(Math.max(0, radius * radius - (radius - depth) * (radius - depth))) : radius;
      if (!(Math.abs(-x * axis.y + y * axis.x - middle) <= half + 0.5)) continue;
      ideal++;
      if (region[y * width + x]) both++;
    }
  }
  return both / (ideal + regionCount - both);
}

/** How far each point lies along `axis`. */
function along(xs, ys, axis) {
  const out = new Float64Array(xs.length);
  for (let k = 0; k < xs.length; k++) out[k] = xs[k] * axis.x + ys[k] * axis.y;
  return out;
}

/** How far behind the front, the foremost point along `axis`, each point lies. */
function depthsOf(xs, ys, axis) {
  const depths = along(xs, ys, axis);
  const front = largest(depths);
  for (let k = 0; k < depths.length; k++) depths[k] = front - depths[k];
  return { front, depths };
}

/** The points less than `limit` behind the front. */
function nearer(xs, ys, depths, limit) {
  const nearX = new Float64Array(xs.length), nearY = new Float64Array(xs.length);
  let count = 0;
  for (let k = 0; k < xs.length; k++) {
    if (!(depths[k] < limit)) continue;
    nearX[count] = xs[k];
    nearY[count++] = ys[k];
  }
  return [nearX.subarray(0, count), nearY.subarray(0, count)];
}

/** The largest value, 0 when there are none. */
function largest(values) {
  let found = values.length ? values[0] : 0;
  for (let k = 1; k < values.length; k++) found = Math.max(found, values[k]);
  return found;
}

/** How many pixels lie at each whole depth behind the front. */
function slicesOf(depths) {
  const counts = new Int32Array(Math.trunc(largest(depths)) + 1);
  for (const depth of depths) counts[Math.trunc(depth)]++;
  return counts;
}

function coordinates(indices, width) {
  const xs = new Float64Array(indices.length), ys = new Float64Array(indices.length);
  for (let k = 0; k < indices.length; k++) {
    xs[k] = indices[k] % width;
    ys[k] = (indices[k] - xs[k]) / width;
  }
  return [xs, ys];
}

function mean(xs, ys) {
  const n = Math.max(xs.length, 1);
  let x = 0, y = 0;
  for (let k = 0; k < xs.length; k++) {
    x += xs[k];
    y += ys[k];
  }
  return { x: x / n, y: y / n };
}

function unit(x, y) {
  const length = Math.max(Math.sqrt(x * x + y * y), 1e-6);
  return { x: x / length, y: y / length };
}

/** The points' covariance: its smaller eigenvalue, and the direction of its larger one. */
function spread(xs, ys) {
  const centre = mean(xs, ys);
  const n = Math.max(xs.length - 1, 1);
  let xx = 0, xy = 0, yy = 0;
  for (let k = 0; k < xs.length; k++) {
    const dx = xs[k] - centre.x, dy = ys[k] - centre.y;
    xx += dx * dx;
    xy += dx * dy;
    yy += dy * dy;
  }
  const a = xx / n, b = xy / n, c = yy / n;
  const root = Math.sqrt(((a - c) / 2) * ((a - c) / 2) + b * b);
  const major = b !== 0 ? unit((a + c) / 2 + root - c, b) : a >= c ? { x: 1, y: 0 } : { x: 0, y: 1 };
  return { least: (a + c) / 2 - root, major };
}

/** Rounded to the nearest whole number, a half to the even one, as Swift's `.toNearestOrEven` is. */
function roundedHalfEven(value) {
  const rounded = Math.round(value);
  return rounded - value === 0.5 && rounded % 2 !== 0 ? rounded - 1 : rounded;
}
