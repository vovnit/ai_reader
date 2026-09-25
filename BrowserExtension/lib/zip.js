// A ZIP archive written in memory: entries stored or deflated, no ZIP64.
// Enough for an EPUB, whose `mimetype` must come first and uncompressed.

const table = Array.from({ length: 256 }, (_, index) => {
  let value = index;
  for (let bit = 0; bit < 8; bit++) value = value & 1 ? 0xedb88320 ^ (value >>> 1) : value >>> 1;
  return value >>> 0;
});

function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) crc = table[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  return (crc ^ 0xffffffff) >>> 0;
}

async function deflate(bytes) {
  const stream = new Blob([bytes]).stream().pipeThrough(new CompressionStream("deflate-raw"));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

export class ZipWriter {
  #parts = [];
  #directory = [];
  #offset = 0;

  /** Adds a file; `contents` is a string or bytes. */
  async add(path, contents, { compress = true } = {}) {
    const bytes = typeof contents === "string" ? new TextEncoder().encode(contents) : contents;
    const name = new TextEncoder().encode(path);
    let stored = bytes;
    let method = 0;
    if (compress && bytes.length > 0) {
      const deflated = await deflate(bytes);
      // A JPEG, say, only grows when deflated; it is stored as it is then.
      if (deflated.length < bytes.length) {
        stored = deflated;
        method = 8;
      }
    }

    const header = new DataView(new ArrayBuffer(30));
    header.setUint32(0, 0x04034b50, true);
    header.setUint16(4, 20, true); // version needed
    header.setUint16(6, 0x0800, true); // names are UTF-8
    header.setUint16(8, method, true);
    header.setUint16(10, 0, true); // time
    header.setUint16(12, 0x21, true); // date: 1 January 1980
    header.setUint32(14, crc32(bytes), true);
    header.setUint32(18, stored.length, true);
    header.setUint32(22, bytes.length, true);
    header.setUint16(26, name.length, true);
    header.setUint16(28, 0, true); // extra field

    const entry = new DataView(new ArrayBuffer(46));
    entry.setUint32(0, 0x02014b50, true);
    entry.setUint16(4, 20, true); // version made by
    for (let i = 4; i < 26; i++) entry.setUint8(i + 2, header.getUint8(i));
    entry.setUint16(28, name.length, true);
    entry.setUint32(42, this.#offset, true);

    this.#parts.push(header, name, stored);
    this.#directory.push(entry, name);
    this.#offset += 30 + name.length + stored.length;
  }

  /** The whole archive. */
  finish(type = "application/zip") {
    const count = this.#directory.length / 2;
    const size = this.#directory.reduce((sum, part) => sum + part.byteLength, 0);
    const end = new DataView(new ArrayBuffer(22));
    end.setUint32(0, 0x06054b50, true);
    end.setUint16(8, count, true);
    end.setUint16(10, count, true);
    end.setUint32(12, size, true);
    end.setUint32(16, this.#offset, true);
    return new Blob([...this.#parts, ...this.#directory, end], { type });
  }
}
