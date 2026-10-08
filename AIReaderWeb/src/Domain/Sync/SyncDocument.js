// What devices agree on: every book's place and group, and every word
// looked up with how it has fared in practice. Each record carries when it
// last changed, and the newer one wins. A deleted word stays as a
// tombstone, so other devices delete it too. On the server the records are
// spread over many files of this form (`SyncParts.js`).
//
// The iOS and Kindle apps share one C++ implementation of this file
// (`Core/Sources/AIReaderCore/Domain/Sync/SyncDocument.cpp`); this is its
// port, and must read, write and merge exactly as it does.
import { compareCodePoints } from "../../Support/Text.js";

export const syncVersion = 1;

/** `{ key, title, author, language, group, chapter, fraction, snippet, updatedAt }`; a place counts only with both chapter and fraction (null otherwise). */
export function bookRecord(fields = {}) {
  return { key: "", title: "", author: "", language: "", group: "", chapter: null, fraction: null, snippet: "", updatedAt: "", ...fields };
}

export function lookupRecord(fields = {}) {
  return {
    word: "", sentence: "", lemma: "", formNote: "", meaning: "", language: "", book: "",
    guessed: false, confidence: 0, lookedUpAt: "", correct: 0, wrong: 0, practicedAt: "", updatedAt: "", deleted: false,
    ...fields,
  };
}

export const lookupKey = (record) => `${record.word}\u0001${record.sentence}`;

const bookFields = Object.keys(bookRecord());
const lookupFields = Object.keys(lookupRecord());
export const sameBook = (a, b) => bookFields.every((field) => a[field] === b[field]);
export const sameLookup = (a, b) => lookupFields.every((field) => a[field] === b[field]);

/** Records in a fixed order, so two documents with the same content compare and encode the same. */
export function sortedDocument(document) {
  return {
    books: [...document.books].sort((a, b) => compareCodePoints(a.key, b.key)),
    lookups: [...document.lookups].sort((a, b) => compareCodePoints(lookupKey(a), lookupKey(b))),
  };
}

export function sameDocument(a, b) {
  const x = sortedDocument(a);
  const y = sortedDocument(b);
  return x.books.length === y.books.length && x.lookups.length === y.lookups.length
    && x.books.every((book, index) => sameBook(book, y.books[index]))
    && x.lookups.every((lookup, index) => sameLookup(lookup, y.lookups[index]));
}

/**
 * Both documents' records, the newer of each pair. A tombstone beats a live
 * record only when it is newer, so a lookup made again after being deleted
 * comes back.
 */
export function mergeDocuments(local, remote) {
  const books = new Map();
  for (const record of [...remote.books, ...local.books]) {
    const known = books.get(record.key);
    if (known && known.updatedAt >= record.updatedAt) continue;
    books.set(record.key, record);
  }
  const lookups = new Map();
  for (const record of [...remote.lookups, ...local.lookups]) {
    const key = lookupKey(record);
    const known = lookups.get(key);
    if (!known) {
      lookups.set(key, record);
      continue;
    }
    let [newest, older] = [known, record];
    if (record.updatedAt > newest.updatedAt) [newest, older] = [record, known];
    // A device without the book still knows the word; keep which book the
    // other device met it in.
    lookups.set(key, newest.book ? newest : { ...newest, book: older.book });
  }
  return sortedDocument({
    books: [...books.values()],
    // A tombstone is only a name and a time, however it was made, so two
    // devices' copies of one compare equal.
    lookups: [...lookups.values()].map((record) =>
      record.deleted ? lookupRecord({ word: record.word, sentence: record.sentence, updatedAt: record.updatedAt, deleted: true }) : record),
  });
}

/** Optional strings travel as null, the way the iOS app writes them. */
const orNull = (value) => (value ? value : null);
const text = (value) => (typeof value === "string" ? value : "");
const number = (value) => (typeof value === "number" ? value : 0);

/** The file's text, members in the C++ app's order, so a diff of two devices' files shows only what differs. */
export function encodeDocument(document) {
  const ordered = sortedDocument(document);
  return JSON.stringify({
    books: ordered.books.map((book) => ({
      author: orNull(book.author),
      chapter: book.chapter ?? null,
      fraction: book.fraction ?? null,
      group: orNull(book.group),
      key: book.key,
      language: orNull(book.language),
      snippet: orNull(book.snippet),
      title: book.title,
      updatedAt: book.updatedAt,
    })),
    lookups: ordered.lookups.map((lookup) => ({
      book: orNull(lookup.book),
      confidence: lookup.confidence,
      correct: lookup.correct,
      deleted: lookup.deleted,
      formNote: lookup.formNote,
      guessed: lookup.guessed,
      language: orNull(lookup.language),
      lemma: lookup.lemma,
      lookedUpAt: lookup.lookedUpAt,
      meaning: lookup.meaning,
      practicedAt: orNull(lookup.practicedAt),
      sentence: lookup.sentence,
      updatedAt: lookup.updatedAt,
      word: lookup.word,
      wrong: lookup.wrong,
    })),
    version: syncVersion,
  });
}

/** The document in the file's text, or null when it is not one. */
export function parseDocument(source) {
  let json;
  try {
    json = JSON.parse(source);
  } catch {
    return null;
  }
  if (!json || typeof json !== "object" || Array.isArray(json)) return null;
  const items = (value) => (Array.isArray(value) ? value.filter((item) => item && typeof item === "object") : []);
  const books = items(json.books)
    .map((item) => bookRecord({
      key: text(item.key), title: text(item.title), author: text(item.author), language: text(item.language),
      group: text(item.group), snippet: text(item.snippet), updatedAt: text(item.updatedAt),
      chapter: typeof item.chapter === "number" ? Math.trunc(item.chapter) : null,
      fraction: typeof item.fraction === "number" ? item.fraction : null,
    }))
    .filter((record) => record.key);
  const lookups = items(json.lookups)
    .map((item) => lookupRecord({
      word: text(item.word), sentence: text(item.sentence), lemma: text(item.lemma), formNote: text(item.formNote),
      meaning: text(item.meaning), language: text(item.language), book: text(item.book),
      guessed: item.guessed === true, confidence: number(item.confidence), lookedUpAt: text(item.lookedUpAt),
      correct: Math.trunc(number(item.correct)), wrong: Math.trunc(number(item.wrong)),
      practicedAt: text(item.practicedAt), updatedAt: text(item.updatedAt), deleted: item.deleted === true,
    }))
    .filter((record) => record.word);
  return { books, lookups };
}
