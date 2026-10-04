// Text from a file in whichever encoding dictionary files turn up in: a
// byte-order mark says, and without one it is UTF-8 unless that does not
// decode — then UTF-16 when every other byte is zero, else Latin-1.
import { gzip, isGzip } from "./Inflate.js";

export function decode(bytes) {
  if (bytes[0] === 0xff && bytes[1] === 0xfe) return new TextDecoder("utf-16le").decode(bytes.subarray(2));
  if (bytes[0] === 0xfe && bytes[1] === 0xff) return new TextDecoder("utf-16be").decode(bytes.subarray(2));
  if (bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) return new TextDecoder().decode(bytes.subarray(3));
  try {
    return new TextDecoder("utf-8", { fatal: true }).decode(bytes);
  } catch {
    const looksUtf16 = bytes.length > 1 && bytes[1] === 0;
    return new TextDecoder(looksUtf16 ? "utf-16le" : "latin1").decode(bytes);
  }
}

/** The file's bytes, ungzipped when gzipped. */
export async function contents(blob) {
  const bytes = new Uint8Array(await blob.arrayBuffer());
  return isGzip(bytes) ? gzip(bytes) : bytes;
}

export async function text(blob) {
  return decode(await contents(blob));
}

/** Lines, with the carriage returns of Windows files dropped. */
export async function lines(blob) {
  return (await text(blob)).split("\n").map((line) => line.replace(/\r$/, ""));
}
