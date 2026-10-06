// Undoes the encodings a PDF stream's text may come in. Image encodings
// are not among them: a page's text never needs its pictures.
import { raw, zlib } from "./Inflate.js";

async function flate(bytes) {
  const inflated = await zlib(bytes).catch(() => null);
  if (inflated) return inflated;
  // A stream with a bad checksum, or none, still holds its text.
  for (const payload of [bytes.subarray(2, bytes.length - 4), bytes.subarray(2)]) {
    const read = await raw(payload).catch(() => null);
    if (read) return read;
  }
  return new Uint8Array();
}

function asciiHex(bytes) {
  const out = [];
  let high = -1;
  for (const byte of bytes) {
    if (byte === 0x3e) break;
    const value = parseInt(String.fromCharCode(byte), 16);
    if (Number.isNaN(value)) continue;
    if (high < 0) {
      high = value;
    } else {
      out.push(high * 16 + value);
      high = -1;
    }
  }
  if (high >= 0) out.push(high * 16);
  return Uint8Array.from(out);
}

function ascii85(bytes) {
  const out = [];
  let group = 0;
  let count = 0;
  for (const byte of bytes) {
    if (byte === 0x7e) break;
    if (byte === 0x7a && count === 0) {
      out.push(0, 0, 0, 0);
      continue;
    }
    if (byte < 0x21 || byte > 0x75) continue;
    group = group * 85 + (byte - 0x21);
    if (++count === 5) {
      out.push((group >>> 24) & 0xff, (group >>> 16) & 0xff, (group >>> 8) & 0xff, group & 0xff);
      group = 0;
      count = 0;
    }
  }
  // A short last group stands for as many bytes as it has digits, less one.
  if (count > 1) {
    for (let pad = count; pad < 5; pad++) group = group * 85 + 84;
    for (let i = 0; i < count - 1; i++) out.push(Math.floor(group / 2 ** (24 - 8 * i)) & 0xff);
  }
  return Uint8Array.from(out);
}

function lzw(bytes) {
  let table = [];
  const reset = () => {
    table = Array.from({ length: 256 }, (_, i) => [i]);
    table.push(null, null); // 256: clear, 257: end
  };
  reset();
  const out = [];
  let previous = null;
  let width = 9;
  let buffer = 0;
  let bits = 0;
  for (const byte of bytes) {
    buffer = ((buffer << 8) | byte) & 0xffffff;
    bits += 8;
    while (bits >= width) {
      const code = (buffer >>> (bits - width)) & ((1 << width) - 1);
      bits -= width;
      if (code === 256) {
        reset();
        width = 9;
        previous = null;
        continue;
      }
      if (code === 257) return Uint8Array.from(out);
      let entry;
      if (code < table.length) entry = table[code];
      else if (previous) entry = [...previous, previous[0]];
      else return Uint8Array.from(out);
      out.push(...entry);
      if (previous) table.push([...previous, entry[0]]);
      previous = entry;
      // The code width grows one entry early, as PDF's encoders do.
      if (table.length + 1 >= 1 << width && width < 12) width++;
    }
  }
  return Uint8Array.from(out);
}

/** The bytes with one filter undone, by its name or abbreviation; null when the filter is not one of these. */
export async function decodeFilter(filter, bytes) {
  if (filter === "FlateDecode" || filter === "Fl") return flate(bytes);
  if (filter === "ASCIIHexDecode" || filter === "AHx") return asciiHex(bytes);
  if (filter === "ASCII85Decode" || filter === "A85") return ascii85(bytes);
  if (filter === "LZWDecode" || filter === "LZW") return lzw(bytes);
  return null;
}
