// A PDF file, read for its text. Its objects are found by scanning the file
// rather than through the cross-reference table, so a damaged or updated
// file reads as well as a clean one. Encrypted files are refused. The same
// reader as the Kindle app's `PdfDocument`.
import { textString } from "./PdfEncodings.js";
import { decodeFilter } from "./PdfFilters.js";
import { PdfLexer, isDict, isName, isStream, member } from "./PdfObject.js";

const isSpace = (byte) => byte === 0x20 || byte === 0x0a || byte === 0x0d || byte === 0x09 || byte === 0x0c || byte === 0x00;
const isDigit = (byte) => byte >= 0x30 && byte <= 0x39;
const ascii = (text) => Uint8Array.from(text, (c) => c.charCodeAt(0));

/** Where `pattern`'s bytes next occur from `from`, or -1. */
function find(bytes, pattern, from = 0) {
  for (let at = bytes.indexOf(pattern[0], from); at >= 0; at = bytes.indexOf(pattern[0], at + 1)) {
    let k = 1;
    while (k < pattern.length && bytes[at + k] === pattern[k]) k++;
    if (k === pattern.length) return at;
  }
  return -1;
}

const objKeyword = ascii("obj");
const endstream = ascii("endstream");
const trailerKeyword = ascii("trailer");

export class PdfDocument {
  #objects = new Map();
  /** Where each object was defined, so a later update's wins. */
  #definedAt = new Map();
  #trailer = { kind: "dict", members: new Map() };
  #catalog = null;
  #pageIndex = new Map();
  /** `{ objectNumber, dictionary, resources, top }`, in reading order. */
  pages = [];

  constructor(bytes) {
    this.bytes = bytes;
  }

  /** Throws with a reason fit to show. */
  static async open(bytes) {
    const header = find(bytes.subarray(0, 1024), ascii("%PDF"));
    if (header < 0) throw new Error("The file is not a PDF.");
    const document = new PdfDocument(bytes);
    document.#scan();
    await document.#readObjectStreams();
    document.#readTrailer();
    if (document.get(document.#trailer, "Encrypt") !== null) {
      throw new Error("The PDF is encrypted, and its text cannot be read here.");
    }
    document.#catalog = document.get(document.#trailer, "Root");
    if (!isDict(document.#catalog)) {
      // No trailer to name it: the catalog is the object that says it is one.
      for (const object of document.#objects.values()) if (isName(member(object, "Type"), "Catalog")) document.#catalog = object;
    }
    document.#collectPages(member(document.#catalog, "Pages"), null, 792, 0, new Set());
    if (!document.pages.length) throw new Error("The PDF has no pages.");
    return document;
  }

  #scan() {
    const bytes = this.bytes;
    let at = 0;
    while ((at = find(bytes, objKeyword, at)) >= 0) {
      const keywordEnd = at + 3;
      let p = at;
      // `12 0 obj`, standing alone: digits, space, digits, space.
      const after = bytes[keywordEnd];
      let alone = (keywordEnd >= bytes.length || isSpace(after) || after === 0x3c || after === 0x5b) && p > 0 && isSpace(bytes[p - 1]);
      while (alone && p > 0 && isSpace(bytes[p - 1])) p--;
      const generationEnd = p;
      while (alone && p > 0 && isDigit(bytes[p - 1])) p--;
      alone &&= p < generationEnd && p > 0 && isSpace(bytes[p - 1]);
      while (alone && p > 0 && isSpace(bytes[p - 1])) p--;
      const numberEnd = p;
      while (alone && p > 0 && isDigit(bytes[p - 1])) p--;
      alone &&= p < numberEnd && (p === 0 || !isDigit(bytes[p - 1]));
      if (!alone) {
        at = keywordEnd;
        continue;
      }
      const number = Number(String.fromCharCode(...bytes.subarray(p, numberEnd)));

      const lexer = new PdfLexer(bytes, keywordEnd);
      let object = lexer.next();
      let end = lexer.position;
      const word = lexer.next();
      if (isDict(object) && word?.kind === "op" && word.text === "stream") {
        let start = lexer.position;
        if (bytes[start] === 0x0d) start++;
        if (bytes[start] === 0x0a) start++;
        let stop = -1;
        // The length, when it is written out and lands on `endstream`;
        // otherwise wherever `endstream` is.
        const length = member(object, "Length");
        if (typeof length === "number" && length >= 0 && start + length <= bytes.length) {
          let q = start + length;
          while (q < bytes.length && isSpace(bytes[q])) q++;
          if (find(bytes.subarray(q, q + 9), endstream) === 0) stop = start + length;
        }
        if (stop < 0) {
          const found = find(bytes, endstream, start);
          stop = found < 0 ? bytes.length : found;
          while (stop > start && (bytes[stop - 1] === 0x0a || bytes[stop - 1] === 0x0d)) stop--;
        }
        object = { kind: "stream", members: object.members, start, length: stop - start };
        end = stop;
      }
      this.#objects.set(number, object);
      this.#definedAt.set(number, at);
      at = Math.max(end, keywordEnd);
    }
  }

  async #readObjectStreams() {
    const streams = [...this.#objects].filter(([, object]) => isStream(object) && isName(member(object, "Type"), "ObjStm"));
    for (const [number, holder] of streams) {
      const where = this.#definedAt.get(number);
      const bytes = await this.contents(holder);
      const count = this.get(holder, "N") ?? 0;
      const first = this.get(holder, "First") ?? 0;
      const header = new PdfLexer(bytes);
      for (let i = 0; i < count && !header.atEnd(); i++) {
        const contained = header.next();
        const offset = header.next();
        if (typeof contained !== "number" || typeof offset !== "number" || first + offset >= bytes.length) continue;
        // An object written again later in the file, by an update, wins.
        if ((this.#definedAt.get(contained) ?? -1) > where) continue;
        this.#objects.set(contained, new PdfLexer(bytes, first + offset).next());
        this.#definedAt.set(contained, where);
      }
    }
  }

  #readTrailer() {
    // Every trailer and cross-reference stream, oldest first, so a later
    // update's entries override an earlier's.
    const trailers = [];
    for (let at = find(this.bytes, trailerKeyword); at >= 0; at = find(this.bytes, trailerKeyword, at + 7)) {
      const dictionary = new PdfLexer(this.bytes, at + 7).next();
      if (isDict(dictionary)) trailers.push([at, dictionary]);
    }
    for (const [number, object] of this.#objects) {
      if (isStream(object) && isName(member(object, "Type"), "XRef")) trailers.push([this.#definedAt.get(number) ?? 0, object]);
    }
    trailers.sort((a, b) => a[0] - b[0]);
    for (const [, trailer] of trailers) {
      for (const key of ["Root", "Info", "Encrypt"]) if (trailer.members.has(key)) this.#trailer.members.set(key, trailer.members.get(key));
    }
  }

  /** `value`, or the object it refers to; null for a missing one. */
  resolve(value) {
    for (let hops = 0; value?.kind === "ref"; hops++) {
      if (hops > 32) return null;
      value = this.#objects.get(value.number) ?? null;
    }
    return value ?? null;
  }

  /** A dictionary's or stream's member, resolved. */
  get(dictionary, key) {
    return this.resolve(member(this.resolve(dictionary), key));
  }

  /** A stream's bytes with its filters undone; empty for an image's encoding, which text never needs. */
  async contents(stream) {
    if (!isStream(stream)) return new Uint8Array();
    let bytes = this.bytes.subarray(stream.start, stream.start + stream.length);
    const filter = this.get(stream, "Filter");
    for (const name of Array.isArray(filter) ? filter : [filter]) {
      const resolved = this.resolve(name);
      if (resolved?.kind !== "name") continue;
      const decoded = await decodeFilter(resolved.text, bytes);
      if (!decoded) return new Uint8Array();
      bytes = decoded;
    }
    return bytes;
  }

  #collectPages(node, resources, top, depth, seen) {
    if (depth > 32) return;
    if (node?.kind === "ref") {
      if (seen.has(node.number)) return;
      seen.add(node.number);
    }
    const page = this.resolve(node);
    const own = this.get(page, "Resources");
    const inherited = isDict(own) ? own : resources;
    const box = this.get(page, "MediaBox");
    if (Array.isArray(box) && box.length === 4) top = Math.max(this.resolve(box[1]), this.resolve(box[3]));
    const kids = this.get(page, "Kids");
    if (Array.isArray(kids)) {
      for (const kid of kids) this.#collectPages(kid, inherited, top, depth + 1, seen);
      return;
    }
    if (!isDict(page)) return;
    const objectNumber = node?.kind === "ref" ? node.number : -1;
    if (objectNumber >= 0) this.#pageIndex.set(objectNumber, this.pages.length);
    this.pages.push({ objectNumber, dictionary: page, resources: inherited, top });
  }

  /**
   * The top level of the outline, the book's chapters, as `{ title, page }`;
   * a lone entry holding the rest gives way to its children.
   */
  outline() {
    const named = new Map(this.get(this.#catalog, "Dests")?.members ?? []);
    this.#namedDestinations(this.get(this.get(this.#catalog, "Names"), "Dests"), named, 0);
    const siblings = (first) => {
      const items = [];
      const seen = new Set();
      for (let link = first; this.resolve(link) !== null && items.length < 10000; link = member(this.resolve(link), "Next")) {
        if (link?.kind === "ref") {
          if (seen.has(link.number)) break;
          seen.add(link.number);
        }
        items.push(this.resolve(link));
      }
      return items;
    };
    let items = siblings(member(this.get(this.#catalog, "Outlines"), "First"));
    if (items.length === 1 && member(items[0], "First") !== null) items = siblings(member(items[0], "First"));
    const bookmarks = [];
    for (const item of items) {
      const page = this.#destinationPage(item, named);
      if (page >= 0) bookmarks.push({ title: textString(this.get(item, "Title")?.text ?? ""), page });
    }
    return bookmarks;
  }

  #namedDestinations(node, named, depth) {
    if (depth > 16) return;
    const names = this.get(node, "Names");
    if (Array.isArray(names)) for (let i = 0; i + 1 < names.length; i += 2) named.set(this.resolve(names[i])?.text, names[i + 1]);
    const kids = this.get(node, "Kids");
    if (Array.isArray(kids)) for (const kid of kids) this.#namedDestinations(kid, named, depth + 1);
  }

  #destinationPage(item, named) {
    let destination = this.get(item, "Dest");
    if (destination === null) {
      const action = this.get(item, "A");
      if (isName(this.get(action, "S"), "GoTo")) destination = this.get(action, "D");
    }
    if (destination?.kind === "name" || destination?.kind === "string") {
      if (!named.has(destination.text)) return -1;
      destination = this.resolve(named.get(destination.text));
    }
    if (isDict(destination)) destination = this.get(destination, "D");
    if (!Array.isArray(destination) || !destination.length) return -1;
    const target = destination[0];
    if (typeof target === "number") return target < this.pages.length ? target : -1;
    return target?.kind === "ref" && this.#pageIndex.has(target.number) ? this.#pageIndex.get(target.number) : -1;
  }

  /** A document information entry, such as `Title` or `Author`. */
  info(key) {
    const value = this.get(this.get(this.#trailer, "Info"), key);
    return value?.kind === "string" ? textString(value.text) : "";
  }

  /** The language the catalog declares, as written there. */
  language() {
    const value = this.get(this.#catalog, "Lang");
    return value?.kind === "string" ? textString(value.text) : "";
  }
}
