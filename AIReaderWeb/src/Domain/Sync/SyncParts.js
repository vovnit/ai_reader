// The sync document kept as many small files rather than one, so that a
// sync reads only the files another device has changed and writes only the
// ones its own changes fall in. A record's file follows from its key alone,
// the same on every device: `00.json` to `ff.json` in the folder
// `aireader-sync`, each one a document of the records that belong there.
//
// A device remembers, for each file, the version the server last gave it
// (its ETag) and a fingerprint of its own records that belong there. A file
// whose version or fingerprint has moved since is due: it is read, merged
// with the records here, and written back when the merge changed it. Two
// devices writing one file at once can overwrite each other; the one
// overwritten finds a new version there on its next sync and writes its
// records again. The one file older versions kept everything in is still
// read whenever it changes, and never written.
//
// The iOS and Kindle apps share one C++ implementation
// (`Core/Sources/AIReaderCore/Domain/Sync/SyncParts.cpp`); this is its port,
// and must name a key's file exactly as it does.
import { encodeDocument, lookupKey, mergeDocuments, sortedDocument } from "./SyncDocument.js";

export const partsFolder = "aireader-sync";
export const oldSyncFile = "aireader-sync.json";

const encoder = new TextEncoder();
const empty = () => ({ books: [], lookups: [] });

/** FNV-1a, 64 bits, over the UTF-8 bytes, as its high and low 32 bits — numbers JavaScript multiplies exactly. */
function fnv(text) {
  let high = 0xcbf29ce4;
  let low = 0x84222325;
  for (const byte of encoder.encode(text)) {
    low = (low ^ byte) >>> 0;
    // Times the prime, 2^40 + 0x1b3, keeping 64 bits.
    const product = low * 0x1b3;
    high = (high * 0x1b3 + low * 0x100 + Math.floor(product / 0x100000000)) >>> 0;
    low = product >>> 0;
  }
  return [high, low];
}

const hex = (value, digits) => value.toString(16).padStart(digits, "0");

/** The file a record with this key belongs in — the top byte, since FNV mixes its low bits least. */
export function partName(key) {
  return `${hex(fnv(key)[0] >>> 24, 2)}.json`;
}

export const isPart = (name) => /^[0-9a-f]{2}\.json$/.test(name);

function byName(document) {
  const parts = new Map();
  const part = (name) => {
    if (!parts.has(name)) parts.set(name, empty());
    return parts.get(name);
  };
  for (const record of document.books) part(partName(record.key)).books.push(record);
  for (const record of document.lookups) part(partName(lookupKey(record))).lookups.push(record);
  return new Map([...parts].sort(([a], [b]) => (a < b ? -1 : 1)));
}

/** The records by file, `[{ name, records }]` in name order; a file no record falls in is left out. */
export function splitDocument(document) {
  return [...byName(document)].map(([name, records]) => ({ name, records: sortedDocument(records) }));
}

/** The parts' records together. */
export function joinParts(parts) {
  return sortedDocument({ books: parts.flatMap((part) => part.records.books), lookups: parts.flatMap((part) => part.records.lookups) });
}

/** A fingerprint of the records, to tell later whether they changed; empty for none. */
export function digest(records) {
  if (!records.books.length && !records.lookups.length) return "";
  const [high, low] = fnv(encodeDocument(records));
  return hex(high, 8) + hex(low, 8);
}

/** Each file's fingerprint of `local`: `[{ name, version: "", digest }]`. */
export function fingerprints(local) {
  return [...byName(local)].map(([name, records]) => ({ name, version: "", digest: digest(records) }));
}

/**
 * The names of the files a sync must read: changed or gone on the server
 * since this device last knew them, holding records that changed here, or
 * holding records of the old file. Files are `{ name, version, digest }`.
 */
export function dueParts(listed, known, local, old) {
  const server = new Map(listed.filter((file) => isPart(file.name)).map((file) => [file.name, file.version]));
  const knew = new Map(known.filter((file) => isPart(file.name)).map((file) => [file.name, file]));
  const here = new Map(fingerprints(local).map((file) => [file.name, file.digest]));
  const due = new Set();
  for (const [name, version] of server) {
    // A server that gives no version leaves every file to be read.
    if (!knew.has(name) || !version || version !== knew.get(name).version) due.add(name);
  }
  for (const [name, file] of knew) {
    if (!server.has(name) || (here.get(name) ?? "") !== file.digest) due.add(name);
  }
  for (const name of here.keys()) if (!knew.has(name)) due.add(name);
  for (const name of byName(old).keys()) due.add(name);
  return [...due].sort();
}

/**
 * Each due file merged: the server's records (`remote`, one `{ name,
 * records }` per due file, with none when the server has none), this
 * device's, and the old file's. In the order of `remote`.
 */
export function mergeParts(remote, local, old) {
  const mine = byName(local);
  const older = byName(old);
  return remote.map((part) => {
    // The server's side last, so where two records tie it keeps its own and
    // is not written again for nothing.
    const here = mergeDocuments(mine.get(part.name) ?? empty(), older.get(part.name) ?? empty());
    return { name: part.name, records: mergeDocuments(here, part.records) };
  });
}

/**
 * What this device knows of the files once `settled` have been dealt with:
 * their entries replace those in `known`, and one with neither a version nor
 * a digest — a file that is not there and need not be — is dropped.
 */
export function rememberParts(known, settled) {
  const files = new Map(known.map((file) => [file.name, file]));
  for (const file of settled) {
    if (!file.version && !file.digest) files.delete(file.name);
    else files.set(file.name, file);
  }
  return [...files.values()].sort((a, b) => (a.name < b.name ? -1 : 1));
}
