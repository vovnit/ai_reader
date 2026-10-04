// Inflate through the platform's own DecompressionStream: raw deflate for
// ZIP entries, zlib for dictionary payloads, gzip for `.dz` and `.gz` files.

async function run(bytes, format) {
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream(format));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

export function raw(bytes) {
  return run(bytes, "deflate-raw");
}

/** Zlib-wrapped data, or nothing when it is not. */
export async function zlib(bytes) {
  // A zlib header: deflate, and a check that makes the first two bytes a multiple of 31.
  if (bytes.length < 2 || (bytes[0] & 0x0f) !== 8 || ((bytes[0] << 8) | bytes[1]) % 31 !== 0) return null;
  try {
    return await run(bytes, "deflate");
  } catch {
    return null;
  }
}

export function isGzip(bytes) {
  return bytes.length > 2 && bytes[0] === 0x1f && bytes[1] === 0x8b;
}

export function gzip(bytes) {
  return run(bytes, "gzip");
}
