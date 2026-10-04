// The library as a sync document, and a sync document written back into
// the library — only the records that differ from what is here. `env`
// holds the stores: `{ library, groups, lookups, cards }`.
import { bookKey } from "../Domain/Books/BookKey.js";
import { samePlace } from "../Domain/Books/ReadingPlace.js";
import {
  bookRecord, lookupKey, lookupRecord, sameBook, sameLookup, sortedDocument,
} from "../Domain/Sync/SyncDocument.js";

/** Everything this device knows, as records. */
export async function exportAll(env) {
  const groups = new Map((await env.groups.all()).map((group) => [group.id, group.name]));
  const keys = new Map();
  const books = (await env.library.all()).map((book) => {
    const key = bookKey(book.title, book.author);
    keys.set(book.id, key);
    return bookRecord({
      key,
      title: book.title,
      author: book.author ?? "",
      language: book.language ?? "",
      group: book.groupId ? groups.get(book.groupId) ?? "" : "",
      chapter: book.place ? book.place.chapter : null,
      fraction: book.place ? book.place.fraction : null,
      snippet: book.place ? book.place.snippet : "",
      updatedAt: book.updatedAt ?? "",
    });
  });

  const cards = new Map((await env.cards.all()).map((card) => [card.lookupId, card]));
  const lookups = (await env.lookups.all()).map((lookup) => {
    const card = cards.get(lookup.id);
    const practicedAt = card?.practicedAt ?? "";
    return lookupRecord({
      word: lookup.word,
      sentence: lookup.sentence,
      lemma: lookup.lemma,
      formNote: lookup.formNote,
      meaning: lookup.meaning,
      language: lookup.language ?? "",
      book: lookup.bookId ? keys.get(lookup.bookId) ?? "" : "",
      guessed: lookup.guessed,
      confidence: lookup.confidence,
      lookedUpAt: lookup.lookedUpAt,
      correct: card?.correct ?? 0,
      wrong: card?.wrong ?? 0,
      practicedAt,
      updatedAt: practicedAt > lookup.lookedUpAt ? practicedAt : lookup.lookedUpAt,
    });
  });
  for (const tombstone of await env.lookups.tombstones()) {
    lookups.push(lookupRecord({ word: tombstone.word, sentence: tombstone.sentence, updatedAt: tombstone.deletedAt, deleted: true }));
  }
  return sortedDocument({ books, lookups });
}

/** Writes the merged document in. Returns `{ books, lookups }`: how many records changed here. */
export async function applyDocument(env, merged) {
  const local = await exportAll(env);
  const applied = { books: 0, lookups: 0 };

  const localBooks = new Map(local.books.map((record) => [record.key, record]));
  const books = new Map((await env.library.all()).map((book) => [bookKey(book.title, book.author), book]));
  for (const record of merged.books) {
    const book = books.get(record.key);
    const known = localBooks.get(record.key);
    if (!book || (known && sameBook(known, record))) continue;
    const groupId = record.group ? await env.groups.named(record.group) : 0;
    await env.library.assignGroup(book.id, groupId, record.updatedAt);
    // A place from elsewhere is worked out into an offset when the book is
    // next opened; the same place again is left alone.
    if (record.chapter !== null && record.fraction !== null) {
      const place = { chapter: record.chapter, fraction: record.fraction, snippet: record.snippet };
      if (!samePlace(place, book.place)) await env.library.savePendingPlace(book.id, place, record.updatedAt);
    }
    applied.books++;
  }

  const localLookups = new Map(local.lookups.map((record) => [lookupKey(record), record]));
  const existing = new Map((await env.lookups.all()).map((lookup) => [lookupKey(lookup), lookup.bookId]));
  for (const record of merged.lookups) {
    const known = localLookups.get(lookupKey(record));
    if (known && sameLookup(known, record)) continue;
    applied.lookups++;
    if (record.deleted) {
      await env.lookups.removeNamed(record.word, record.sentence);
      await env.lookups.addTombstone({ word: record.word, sentence: record.sentence, deletedAt: record.updatedAt });
      continue;
    }
    await env.lookups.removeTombstone(record.word, record.sentence);
    const lookedUpAt = record.lookedUpAt || record.updatedAt;
    const id = await env.lookups.upsert({
      word: record.word,
      sentence: record.sentence,
      lemma: record.lemma,
      formNote: record.formNote,
      meaning: record.meaning,
      language: record.language || null,
      // A book this device does not have leaves the lookup attached to
      // whatever it was attached to here.
      bookId: books.get(record.book)?.id ?? existing.get(lookupKey(record)) ?? null,
      guessed: record.guessed,
      confidence: record.confidence,
      lookedUpAt,
    });
    if (id && (record.correct + record.wrong > 0 || record.practicedAt)) {
      await env.cards.set(id, record.correct, record.wrong, record.practicedAt || lookedUpAt);
    }
  }

  // A group nothing is left in has been dissolved somewhere.
  for (const group of await env.groups.all()) {
    if (!(await env.library.inGroup(group.id)).length) await env.groups.remove(group.id);
  }
  return applied;
}
