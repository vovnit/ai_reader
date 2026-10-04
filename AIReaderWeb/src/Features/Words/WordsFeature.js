// Every word looked up so far, newest first — the vocabulary the reader has
// actually met, rather than a list someone else chose. From one book, its
// lookups; from the shelf, all of them.
import { ankiText } from "../../Domain/Cards/AnkiExport.js";
import { cardFromLookup } from "../../Domain/Cards/Card.js";
import { Feature } from "../Common/Feature.js";

export class WordsFeature extends Feature {
  lookups = [];
  #env;

  /** `book` narrows the list to it; null lists every word met. */
  constructor(env, book) {
    super();
    this.#env = env;
    this.book = book;
  }

  async reload() {
    this.lookups = await this.#env.lookups.all(this.book?.id ?? 0);
    this.changed();
  }

  async remove(lookup) {
    await this.#env.lookups.remove(lookup.id);
    await this.reload();
  }

  /** The listed words as the file Anki imports: `{ name, text }`. One book's go to a deck of their own under the app's. */
  ankiFile() {
    const deck = this.book ? `AIReader::${this.book.title}` : "AIReader";
    return { name: `${deck.replace("::", " - ").replace(/[\\/:*?"<>|]/g, " ")}.txt`, text: ankiText(this.lookups.map(cardFromLookup), deck) };
  }
}
