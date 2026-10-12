// A picture as a grid of red, green and blue values, for looking at its
// pixels one by one. The same as the iOS app's `RGBImage`, its arithmetic
// in 32-bit floats as there, so the two shrink a photo alike.

// Each step is rounded to a 32-bit float, as Swift's `Float` rounds it;
// a typed array's element is rounded as it is stored.
const f = Math.fround;

export class RgbImage {
  /** `values` are three a pixel, 0 to 255, row after row from the top left: a `Float32Array`. */
  constructor(width, height, values) {
    this.width = width;
    this.height = height;
    this.values = values;
  }

  red(index) {
    return this.values[3 * index];
  }

  green(index) {
    return this.values[3 * index + 1];
  }

  blue(index) {
    return this.values[3 * index + 2];
  }

  /** The picture from a canvas's RGBA bytes, as `ImageData` holds them; the alpha is dropped. */
  static fromRgba(data, width, height) {
    const values = new Float32Array(width * height * 3);
    for (let i = 0; i < width * height; i++) {
      values[3 * i] = data[4 * i];
      values[3 * i + 1] = data[4 * i + 1];
      values[3 * i + 2] = data[4 * i + 2];
    }
    return new RgbImage(width, height, values);
  }

  /**
   * The picture shrunk so its longer side is `longSide`, each new pixel the
   * average of the area of the old ones it covers.
   */
  shrunk(longSide) {
    const { width, height, values } = this;
    const scale = Math.min(1, longSide / Math.max(width, height));
    const newWidth = Math.max(1, Math.round(width * scale));
    const newHeight = Math.max(1, Math.round(height * scale));
    const across = shares(width, newWidth);
    const down = shares(height, newHeight);

    const rows = new Float32Array(newWidth * height * 3);
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < newWidth; x++) {
        const out = (y * newWidth + x) * 3;
        for (let k = across.starts[x]; k < across.starts[x + 1]; k++) {
          const from = (y * width + across.olds[k]) * 3;
          const weight = across.weights[k];
          rows[out] += f(weight * values[from]);
          rows[out + 1] += f(weight * values[from + 1]);
          rows[out + 2] += f(weight * values[from + 2]);
        }
      }
    }
    const rowLength = newWidth * 3;
    const shrunk = new Float32Array(newWidth * newHeight * 3);
    for (let y = 0; y < newHeight; y++) {
      for (let k = down.starts[y]; k < down.starts[y + 1]; k++) {
        const from = down.olds[k] * rowLength;
        const weight = down.weights[k];
        for (let i = 0; i < rowLength; i++) shrunk[y * rowLength + i] += f(weight * rows[from + i]);
      }
    }
    return new RgbImage(newWidth, newHeight, shrunk);
  }
}

/**
 * For each new pixel along a side, the old pixels it covers and how much of
 * it each one fills: new pixel `i`'s are `olds` and `weights` from
 * `starts[i]` to `starts[i + 1]`.
 */
function shares(old, count) {
  const step = old / count;
  const starts = [0];
  const olds = [];
  const weights = [];
  for (let i = 0; i < count; i++) {
    const start = i * step;
    const end = start + step;
    const last = Math.min(old, Math.ceil(end));
    for (let j = Math.trunc(start); j < last; j++) {
      const cover = Math.min(end, j + 1) - Math.max(start, j);
      if (cover > 0) {
        olds.push(j);
        weights.push(cover / step);
      }
    }
    starts.push(olds.length);
  }
  return { starts: Int32Array.from(starts), olds: Int32Array.from(olds), weights: Float32Array.from(weights) };
}
