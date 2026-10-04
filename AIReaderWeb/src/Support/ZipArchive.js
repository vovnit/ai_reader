// Reads a ZIP archive in memory from its central directory: stored and
// deflated entries, which is all an EPUB holds. No ZIP64.
import { raw } from "./Inflate.js";

const decoder = new TextDecoder();

export class ZipArchive {
  #bytes;
  #view;
  /** Entry name → where its local header is and how it is stored. */
  #entries = new Map();

  constructor(bytes) {
    this.#bytes = bytes;
    this.#view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    this.#readDirectory();
  }

  /** Throws when the bytes are not a ZIP archive. */
  static async open(blob) {
    return new ZipArchive(new Uint8Array(await blob.arrayBuffer()));
  }

  #readDirectory() {
    const view = this.#view;
    // The end record sits in the last 64 KB, after an optional comment.
    let end = -1;
    for (let i = this.#bytes.length - 22; i >= Math.max(0, this.#bytes.length - 65557); i--) {
      if (view.getUint32(i, true) === 0x06054b50) {
        end = i;
        break;
      }
    }
    if (end < 0) throw new Error("The file is not a ZIP archive.");
    const count = view.getUint16(end + 10, true);
    let offset = view.getUint32(end + 16, true);
    for (let i = 0; i < count && offset + 46 <= this.#bytes.length; i++) {
      if (view.getUint32(offset, true) !== 0x02014b50) break;
      const method = view.getUint16(offset + 10, true);
      const compressedSize = view.getUint32(offset + 20, true);
      const nameLength = view.getUint16(offset + 28, true);
      const extraLength = view.getUint16(offset + 30, true);
      const commentLength = view.getUint16(offset + 32, true);
      const localHeader = view.getUint32(offset + 42, true);
      const name = decoder.decode(this.#bytes.subarray(offset + 46, offset + 46 + nameLength));
      this.#entries.set(name, { method, compressedSize, localHeader });
      offset += 46 + nameLength + extraLength + commentLength;
    }
  }

  names() {
    return [...this.#entries.keys()];
  }

  has(name) {
    return this.#entries.has(name);
  }

  /** The entry's name as stored, matched without regard to case when it differs. */
  find(name) {
    if (this.#entries.has(name)) return name;
    const lowered = name.toLowerCase();
    return this.names().find((candidate) => candidate.toLowerCase() === lowered) ?? null;
  }

  /** The entry's bytes, or null when there is no such entry or it cannot be read. */
  async bytes(name) {
    const entry = this.#entries.get(name);
    if (!entry) return null;
    const view = this.#view;
    const header = entry.localHeader;
    if (header + 30 > this.#bytes.length || view.getUint32(header, true) !== 0x04034b50) return null;
    const start = header + 30 + view.getUint16(header + 26, true) + view.getUint16(header + 28, true);
    const data = this.#bytes.subarray(start, start + entry.compressedSize);
    if (entry.method === 0) return data;
    if (entry.method !== 8) return null;
    try {
      return await raw(data);
    } catch {
      return null;
    }
  }

  async text(name) {
    const bytes = await this.bytes(name);
    return bytes ? decoder.decode(bytes) : null;
  }
}
