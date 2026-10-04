// Finding a phrase in the book being read and the rest of its group. The
// reader's own search covers the whole text — what to know ahead of time is
// their choice; only the model is kept to the pages read.
import { Feature } from "../Common/Feature.js";

/** How many hits are listed. */
export const searchLimit = 100;

export class SearchFeature extends Feature {
  query = "";
  hits = [];
  isSearching = false;
  error = "";

  constructor(corpus) {
    super();
    this.corpus = corpus;
  }

  get severalBooks() {
    return !!this.corpus?.severalBooks;
  }

  /** Runs a search; an empty query, or one made while searching, is ignored. */
  async search(query) {
    const wanted = query.trim();
    if (!wanted || this.isSearching || !this.corpus) return;
    Object.assign(this, { query: wanted, isSearching: true, error: "" });
    this.changed();
    this.hits = await this.corpus.search(wanted, searchLimit);
    this.error = this.corpus.errors().join("\n");
    this.isSearching = false;
    this.changed();
  }
}
