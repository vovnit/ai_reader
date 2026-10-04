// Reads an SQLite database file without SQLite: the file format's b-trees,
// walked page by page out of a Blob, so a dictionary pack is searched where
// it lies instead of being loaded whole. Read-only; enough for the packs:
// table scans, a row by rowid, and the rows of an index (or a WITHOUT ROWID
// table) whose first columns equal given values. Text must be UTF-8.

const decoder = new TextDecoder();
const encoder = new TextEncoder();

/** Stops a walk once rows have passed the wanted ones. */
const stop = Symbol("stop");

export class SqliteFile {
  #blob;
  #pageSize;
  #usable;
  #pages = new Map();

  constructor(blob, pageSize, usable) {
    this.#blob = blob;
    this.#pageSize = pageSize;
    this.#usable = usable;
  }

  static async isDatabase(blob) {
    const head = new Uint8Array(await blob.slice(0, 16).arrayBuffer());
    return decoder.decode(head) === "SQLite format 3\0";
  }

  /** Throws when the blob is not an SQLite file this reader can read. */
  static async open(blob) {
    if (!(await SqliteFile.isDatabase(blob))) throw new Error("Not an SQLite database.");
    const header = new DataView(await blob.slice(0, 100).arrayBuffer());
    const size = header.getUint16(16);
    const pageSize = size === 1 ? 65536 : size;
    if (header.getUint32(56) > 1) throw new Error("The database is not UTF-8.");
    return new SqliteFile(blob, pageSize, pageSize - header.getUint8(20));
  }

  async #page(number) {
    let page = this.#pages.get(number);
    if (page) return page;
    const start = (number - 1) * this.#pageSize;
    page = new Uint8Array(await this.#blob.slice(start, start + this.#pageSize).arrayBuffer());
    // A lookup touches a few dozen pages; keep the recent ones, which are
    // mostly the top of each tree.
    if (this.#pages.size > 512) this.#pages.delete(this.#pages.keys().next().value);
    this.#pages.set(number, page);
    return page;
  }

  /** The rows of `sqlite_schema`: `{ type, name, tableName, rootPage, sql }`. */
  async schema() {
    const rows = [];
    await this.#walkTable(1, (rowid, [type, name, tableName, rootPage, sql]) => {
      rows.push({ type, name, tableName, rootPage, sql });
    });
    return rows;
  }

  /** Every row of a rowid table, as `[rowid, values]`. */
  async tableRows(rootPage) {
    const rows = [];
    await this.#walkTable(rootPage, (rowid, values) => rows.push([rowid, values]));
    return rows;
  }

  /** The values of the row with this rowid, or null. */
  async row(rootPage, rowid) {
    let number = rootPage;
    for (;;) {
      const node = await this.#node(number);
      if (node.leaf) {
        for (let i = 0; i < node.count; i++) {
          const [size, afterSize] = varint(node.page, node.cell(i));
          const [key, afterKey] = varint(node.page, afterSize);
          if (key === rowid) return record(await this.#payload(node.page, afterKey, size, true));
        }
        return null;
      }
      // The first child whose key is at least the rowid holds it.
      let next = node.right;
      for (let i = 0; i < node.count; i++) {
        const cell = node.cell(i);
        if (varint(node.page, cell + 4)[0] >= rowid) {
          next = readUint32(node.page, cell);
          break;
        }
      }
      number = next;
    }
  }

  /**
   * The records of an index b-tree, in order, whose first columns equal
   * `prefix` — numbers and strings, compared the way SQLite's BINARY
   * collation does. A WITHOUT ROWID table is such a tree, its primary key
   * first.
   */
  async indexRows(rootPage, prefix) {
    const rows = [];
    const wanted = prefix.map((value) => (typeof value === "string" ? { text: encoder.encode(value) } : value));
    await this.#walkIndex(rootPage, wanted, rows);
    return rows.map((values) => values.map((value) => (value?.text ? decoder.decode(value.text) : value)));
  }

  async #walkIndex(number, wanted, rows) {
    const node = await this.#node(number);
    const recordAt = async (i) => {
      const [size, start] = varint(node.page, node.cell(i) + (node.leaf ? 0 : 4));
      return record(await this.#payload(node.page, start, size, false), true);
    };
    // The first cell not below the prefix: everything before it is smaller,
    // and so is the child to its left.
    let low = 0;
    let high = node.count;
    while (low < high) {
      const middle = (low + high) >> 1;
      if (comparePrefix(await recordAt(middle), wanted) < 0) low = middle + 1;
      else high = middle;
    }
    for (let i = low; i < node.count; i++) {
      if (!node.leaf && (await this.#walkIndex(readUint32(node.page, node.cell(i)), wanted, rows)) === stop) return stop;
      const values = await recordAt(i);
      if (comparePrefix(values, wanted) > 0) return stop;
      rows.push(values);
    }
    if (!node.leaf) return this.#walkIndex(node.right, wanted, rows);
  }

  async #walkTable(number, visit) {
    const node = await this.#node(number);
    for (let i = 0; i < node.count; i++) {
      const cell = node.cell(i);
      if (!node.leaf) {
        await this.#walkTable(readUint32(node.page, cell), visit);
        continue;
      }
      const [size, afterSize] = varint(node.page, cell);
      const [rowid, afterKey] = varint(node.page, afterSize);
      visit(rowid, record(await this.#payload(node.page, afterKey, size, true)));
    }
    if (!node.leaf) await this.#walkTable(node.right, visit);
  }

  async #node(number) {
    const page = await this.#page(number);
    const header = number === 1 ? 100 : 0;
    const type = page[header];
    if (![2, 5, 10, 13].includes(type)) throw new Error(`Page ${number} is not a b-tree page.`);
    const leaf = type === 10 || type === 13;
    const count = (page[header + 3] << 8) | page[header + 4];
    const pointers = header + (leaf ? 8 : 12);
    return {
      page,
      leaf,
      count,
      right: leaf ? 0 : readUint32(page, header + 8),
      cell: (i) => (page[pointers + 2 * i] << 8) | page[pointers + 2 * i + 1],
    };
  }

  /** A cell's payload, gathered from its overflow pages when it spills. */
  async #payload(page, start, size, table) {
    const usable = this.#usable;
    const most = table ? usable - 35 : Math.floor(((usable - 12) * 64) / 255) - 23;
    if (size <= most) return page.subarray(start, start + size);
    const least = Math.floor(((usable - 12) * 32) / 255) - 23;
    let local = least + ((size - least) % (usable - 4));
    if (local > most) local = least;
    const payload = new Uint8Array(size);
    payload.set(page.subarray(start, start + local));
    let filled = local;
    let next = readUint32(page, start + local);
    while (filled < size && next) {
      const overflow = await this.#page(next);
      const chunk = Math.min(usable - 4, size - filled);
      payload.set(overflow.subarray(4, 4 + chunk), filled);
      filled += chunk;
      next = readUint32(overflow, 0);
    }
    return payload;
  }
}

/** A record's values; text is `{ text: bytes }` when `raw`, for comparing. */
function record(bytes, raw = false) {
  const [headerSize, afterHeaderSize] = varint(bytes, 0);
  const types = [];
  for (let at = afterHeaderSize; at < headerSize; ) {
    const [type, next] = varint(bytes, at);
    types.push(type);
    at = next;
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const values = [];
  let at = headerSize;
  for (const type of types) {
    if (type === 0) {
      values.push(null);
    } else if (type <= 6) {
      const length = [0, 1, 2, 3, 4, 6, 8][type];
      let value = 0;
      for (let i = 0; i < length; i++) value = value * 256 + bytes[at + i];
      values.push(bytes[at] & 0x80 ? value - 2 ** (8 * length) : value);
      at += length;
    } else if (type === 7) {
      values.push(view.getFloat64(at));
      at += 8;
    } else if (type === 8 || type === 9) {
      values.push(type - 8);
    } else if (type >= 12) {
      const length = (type - (type % 2 ? 13 : 12)) / 2;
      const slice = bytes.slice(at, at + length);
      values.push(type % 2 === 0 ? slice : raw ? { text: slice } : decoder.decode(slice));
      at += length;
    }
  }
  return values;
}

/** Orders a record's first columns against the wanted ones: null, numbers, text, blobs. */
function comparePrefix(values, wanted) {
  for (let i = 0; i < wanted.length; i++) {
    const a = values[i] ?? null;
    const b = wanted[i];
    const order = rank(a) - rank(b) || (typeof a === "number" ? a - b : a?.text ? compareBytes(a.text, b.text) : 0);
    if (order) return order;
  }
  return 0;
}

function rank(value) {
  if (value === null) return 0;
  if (typeof value === "number") return 1;
  return value.text ? 2 : 3;
}

function compareBytes(a, b) {
  const length = Math.min(a.length, b.length);
  for (let i = 0; i < length; i++) if (a[i] !== b[i]) return a[i] - b[i];
  return a.length - b.length;
}

function readUint32(bytes, at) {
  return bytes[at] * 0x1000000 + (bytes[at + 1] << 16) + (bytes[at + 2] << 8) + bytes[at + 3];
}

/** SQLite's variable-length integer at `at`, and where the next field starts. */
function varint(bytes, at) {
  let value = 0;
  for (let i = 0; i < 8; i++) {
    const byte = bytes[at + i];
    value = value * 128 + (byte & 0x7f);
    if (!(byte & 0x80)) return [value, at + i + 1];
  }
  return [value * 256 + bytes[at + 8], at + 9];
}
