// The text of the book being read and of the other books in its group,
// read once, when first searched. A search is what the reader, a lookup,
// an X-ray or a conversation runs over it.
import { findInText } from "../Domain/Search/BookSearch.js";
import { loadDocument } from "./EpubLoader.js";

export class BookCorpus {
  #library;
  #chapters = new Map();
  #errors = new Map();

  /** `books` in the order they are to be searched: the open book first. */
  constructor(books, library) {
    this.books = books;
    this.#library = library;
  }

  get severalBooks() {
    return this.books.length > 1;
  }

  /** Hands over chapters already loaded, so that book is not read again. */
  provide(bookId, chapters) {
    this.#chapters.set(bookId, chapters);
    this.#errors.delete(bookId);
  }

  async #loadMissing() {
    for (const book of this.books) await this.#load(book);
  }

  async #load(book) {
    if (this.#chapters.has(book.id) || this.#errors.has(book.id)) return;
    try {
      const file = await this.#library.file(book.id);
      if (!file) throw new Error("its file is missing.");
      const document = await loadDocument(file, { withImages: false });
      this.#chapters.set(book.id, document.chapters.map((chapter) => chapter.text));
    } catch (error) {
      this.#errors.set(book.id, `${book.title}: ${error.message}`);
    }
  }

  /** One chapter's text, or "" when the book cannot be read. */
  async chapterText(bookId, chapter) {
    const book = this.books.find((candidate) => candidate.id === bookId);
    if (book) await this.#load(book);
    return this.#chapters.get(bookId)?.[chapter] ?? "";
  }

  /**
   * Hits book by book, in reading order, at most `limit`:
   * `{ bookId, bookTitle, chapter, offset, excerpt, matchStart, matchEnd }`.
   * With `upTo` — `{ bookId, chapter, offset }` — that book is searched only
   * as far as the reader has seen, and the others in full.
   */
  async search(query, limit, upTo = null) {
    await this.#loadMissing();
    const hits = [];
    for (const book of this.books) {
      const chapters = this.#chapters.get(book.id);
      if (!chapters) continue;
      const bounded = upTo && upTo.bookId === book.id;
      for (let chapter = 0; chapter < chapters.length && hits.length < limit; chapter++) {
        if (bounded && chapter > upTo.chapter) break;
        for (const hit of findInText(chapters[chapter], query, chapter, limit - hits.length)) {
          if (bounded && chapter === upTo.chapter && hit.offset >= upTo.offset) break;
          hits.push({ ...hit, bookId: book.id, bookTitle: book.title });
        }
      }
    }
    return hits;
  }

  /** Why a book could not be read, if one could not; checked after a search. */
  errors() {
    return [...this.#errors.values()];
  }
}
