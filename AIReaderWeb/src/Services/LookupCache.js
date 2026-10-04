// Remembers explanations so the same word in the same sentence is paid for
// once — and keeps the list of every word met so far. A lookup is
// `{ id, word, sentence, lemma, formNote, meaning, language, bookId,
//    guessed, confidence, lookedUpAt }`. A deleted one leaves a tombstone,
// `{ word, sentence, deletedAt }`, so a sync does not bring it back.
import { now } from "../Support/Text.js";

const newestFirst = (a, b) => (a.lookedUpAt < b.lookedUpAt ? 1 : a.lookedUpAt > b.lookedUpAt ? -1 : b.id - a.id);

export class LookupCache {
  #db;

  constructor(db) {
    this.#db = db;
  }

  #named(word, sentence) {
    return this.#db.getByIndex("lookups", "wordSentence", [word, sentence]);
  }

  /** The stored explanation for `{ word, sentence }`, or null. */
  async cached(context) {
    const lookup = await this.#named(context.word, context.sentence);
    if (!lookup) return null;
    const { lemma, formNote, meaning, guessed, confidence } = lookup;
    return { lemma, formNote, meaning, guessed, confidence };
  }

  /** Stores an explanation for `{ word, sentence, language, bookId }`, over any before it. */
  async save(context, explanation) {
    await this.removeNamed(context.word, context.sentence);
    await this.removeTombstone(context.word, context.sentence);
    await this.#db.put("lookups", {
      word: context.word,
      sentence: context.sentence,
      lemma: explanation.lemma,
      formNote: explanation.formNote,
      meaning: explanation.meaning,
      language: context.language || null,
      bookId: context.bookId || null,
      guessed: explanation.guessed,
      confidence: explanation.confidence,
      lookedUpAt: now(),
    });
  }

  /** Forgets a lookup, and remembers that it was forgotten. */
  async remove(id) {
    const lookup = await this.#db.get("lookups", id);
    if (!lookup) return;
    await this.addTombstone({ word: lookup.word, sentence: lookup.sentence, deletedAt: now() });
    await this.#db.delete("lookups", id);
    await this.#db.delete("cardPractice", id);
  }

  /** Every lookup, newest first; `bookId` narrows it to one book. */
  async all(bookId = 0) {
    const lookups = bookId ? await this.#db.getAll("lookups", "bookId", bookId) : await this.#db.getAll("lookups");
    return lookups.sort(newestFirst);
  }

  tombstones() {
    return this.#db.getAll("lookupTombstones");
  }

  addTombstone(tombstone) {
    return this.#db.put("lookupTombstones", tombstone);
  }

  removeTombstone(word, sentence) {
    return this.#db.delete("lookupTombstones", [word, sentence]);
  }

  /** Writes a lookup from another device over the one here with the same word and sentence, or as a new one. Returns its id. */
  async upsert(lookup) {
    const existing = await this.#named(lookup.word, lookup.sentence);
    const { id, ...fields } = lookup;
    return this.#db.put("lookups", existing ? { ...fields, id: existing.id } : fields);
  }

  /** Deletes by name, with its practice record; true when there was one. */
  async removeNamed(word, sentence) {
    const existing = await this.#named(word, sentence);
    if (!existing) return false;
    await this.#db.delete("lookups", existing.id);
    await this.#db.delete("cardPractice", existing.id);
    return true;
  }
}
