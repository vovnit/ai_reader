// Every lookup is a flash card; this keeps how each has fared in practice:
// `{ lookupId, correct, wrong, practicedAt }`.
import { cardFromLookup } from "../Domain/Cards/Card.js";
import { now } from "../Support/Text.js";

export class CardStore {
  #db;
  #lookups;

  constructor(db, lookups) {
    this.#db = db;
    this.#lookups = lookups;
  }

  /** The cards of every lookup, or of one book's, with their practice. */
  async all(bookId = 0) {
    const practice = new Map((await this.#db.getAll("cardPractice")).map((row) => [row.lookupId, row]));
    return (await this.#lookups.all(bookId)).map((lookup) => {
      const card = cardFromLookup(lookup);
      const row = practice.get(lookup.id);
      return row ? { ...card, correct: row.correct, wrong: row.wrong, practicedAt: row.practicedAt } : card;
    });
  }

  async record(lookupId, correct) {
    const row = (await this.#db.get("cardPractice", lookupId)) ?? { lookupId, correct: 0, wrong: 0 };
    await this.#db.put("cardPractice", {
      lookupId,
      correct: row.correct + (correct ? 1 : 0),
      wrong: row.wrong + (correct ? 0 : 1),
      practicedAt: now(),
    });
  }

  /** The record as another device has it. */
  set(lookupId, correct, wrong, practicedAt) {
    return this.#db.put("cardPractice", { lookupId, correct, wrong, practicedAt });
  }
}
