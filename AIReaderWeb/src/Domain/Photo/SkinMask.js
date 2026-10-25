// The pixels of a shrunk photo that could be a finger. Colours are judged
// against the page's own white, so skin stays skin under warm, cool or
// coloured light, and texture against the print's own, so the size and
// sharpness of the photo do not matter. The same as the iOS app's
// `SkinMask`, in 32-bit floats as there, so a pixel near a threshold is
// judged alike.

// Each step is rounded to a 32-bit float, as Swift's `Float` rounds it;
// a typed array's element is rounded as it is stored.
const f = Math.fround;

/**
 * `{ width, height, skin, smooth, brightness }` for an `RgbImage`, or null
 * when no word lies inside it. `words` are the word boxes,
 * `{ x, y, width, height }` in the shrunk photo's pixels. `skin` is 1 where
 * red is the strongest, with a skin's hue and saturation once balanced to
 * the page's white: never paper, its shadows, ink or tiles. `smooth` is 1
 * where the photo is far less textured than the print: never words or a
 * picture's detail. `brightness` is the brightest channel, against the
 * page's white.
 */
export function skinMask(image, words) {
  const { width, height, values } = image;
  const count = width * height;
  const inWords = cover(words, width, height);
  const wordPixels = [];
  for (let i = 0; i < count; i++) if (inWords[i]) wordPixels.push(i);
  if (!wordPixels.length) return null;

  // The page's white is the brightest tenth of what lies inside the word
  // boxes, which is mostly the paper between strokes.
  const sums = new Float32Array(wordPixels.length);
  for (let k = 0; k < wordPixels.length; k++) {
    const i = wordPixels[k];
    sums[k] = f(values[3 * i] + values[3 * i + 1]) + values[3 * i + 2];
  }
  const brightest = percentile(sums.slice().sort(), 0.9);
  let red = 0, green = 0, blue = 0, white = 0;
  for (let k = 0; k < wordPixels.length; k++) {
    if (sums[k] < brightest) continue;
    const i = wordPixels[k];
    red = f(red + values[3 * i]);
    green = f(green + values[3 * i + 1]);
    blue = f(blue + values[3 * i + 2]);
    white++;
  }
  const whiteRed = Math.max(1, f(red / white));
  const whiteGreen = Math.max(1, f(green / white));
  const whiteBlue = Math.max(1, f(blue / white));

  const balanced = new Float32Array(count * 3);
  const skin = new Uint8Array(count);
  const brightness = new Float32Array(count);
  const tiny = f(1e-6), saturation = f(0.15);
  for (let i = 0; i < count; i++) {
    const r = f(values[3 * i] / whiteRed), g = f(values[3 * i + 1] / whiteGreen), b = f(values[3 * i + 2] / whiteBlue);
    balanced[3 * i] = r;
    balanced[3 * i + 1] = g;
    balanced[3 * i + 2] = b;
    const low = Math.min(g, b);
    const hue = f(f(60 * f(g - b)) / Math.max(f(r - low), tiny));
    skin[i] = r >= Math.max(g, b) && hue > -25 && hue < 40 && f(r - low) > f(saturation * r) ? 1 : 0;
    brightness[i] = Math.max(r, g, b);
  }

  const texture = textureOf(balanced, width, height);
  const printTexture = median(wordPixels.map((i) => texture[i]));
  const limit = f(f(0.4) * printTexture);
  const smooth = new Uint8Array(count);
  for (let i = 0; i < count; i++) smooth[i] = texture[i] < limit ? 1 : 0;
  return { width, height, skin, smooth, brightness };
}

/**
 * The colour step across each pixel, to the left and right and up and
 * down, averaged over its 3 by 3 neighbourhood.
 */
function textureOf(rgb, width, height) {
  const step = new Float32Array(width * height);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const i = y * width + x;
      let sum = 0;
      for (let channel = 0; channel < 3; channel++) {
        if (x > 0 && x < width - 1) sum = f(sum + Math.abs(f(rgb[3 * (i + 1) + channel] - rgb[3 * (i - 1) + channel])));
        if (y > 0 && y < height - 1) sum = f(sum + Math.abs(f(rgb[3 * (i + width) + channel] - rgb[3 * (i - width) + channel])));
      }
      step[i] = sum;
    }
  }
  const averaged = new Float32Array(width * height);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      let sum = 0;
      for (let dy = -1; dy <= 1; dy++) {
        const row = Math.min(Math.max(y + dy, 0), height - 1) * width;
        for (let dx = -1; dx <= 1; dx++) sum = f(sum + step[row + Math.min(Math.max(x + dx, 0), width - 1)]);
      }
      averaged[y * width + x] = sum / 9;
    }
  }
  return averaged;
}

/** 1 for each pixel inside one of `boxes`, `{ x, y, width, height }`. */
export function cover(boxes, width, height) {
  const covered = new Uint8Array(width * height);
  for (const box of boxes) {
    const x0 = Math.max(0, Math.trunc(box.x)), x1 = Math.min(width, Math.ceil(box.x + box.width));
    const y0 = Math.max(0, Math.trunc(box.y)), y1 = Math.min(height, Math.ceil(box.y + box.height));
    for (let y = y0; y < y1; y++) covered.fill(1, y * width + x0, y * width + Math.max(x0, x1));
  }
  return covered;
}

/** Between the two nearest values, as numpy's percentile does; `sorted` holds 32-bit floats. */
export function percentile(sorted, fraction) {
  const position = f(f(fraction) * (sorted.length - 1));
  const below = Math.trunc(position);
  const above = Math.min(below + 1, sorted.length - 1);
  return f(sorted[below] + f(f(sorted[above] - sorted[below]) * f(position - below)));
}

/** The middle value, or the mean of the middle two, as a 32-bit float. */
export function median(values) {
  const sorted = new Float32Array(values).sort();
  const middle = Math.floor(sorted.length / 2);
  return sorted.length % 2 === 1 ? sorted[middle] : f(f(sorted[middle - 1] + sorted[middle]) / 2);
}
