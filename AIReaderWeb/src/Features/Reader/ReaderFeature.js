// Reading one book: load its chapters, keep to a page, remember the
// position, and turn a tap into a word to look up. The browser lays the
// chapter out; the view hands back where its pages start (`laidOut`), and
// everything else is decided here. Holds the corpus — this book and its
// group — that searches run over.
import { contentsEntryAt } from "../../Domain/Books/EpubNavigation.js";
import { placeAt, resolvePlace } from "../../Domain/Books/ReadingPlace.js";
import { selectionAt } from "../../Domain/Reading/WordContext.js";
import { BookCorpus } from "../../Services/BookCorpus.js";
import { loadDocument } from "../../Services/EpubLoader.js";
import { Feature } from "../Common/Feature.js";

export class ReaderFeature extends Feature {
  status = "loading";
  error = "";
  document = null;
  chapter = 0;
  page = 0;
  pageCount = 0;
  /** Counts up whenever the chapter must be laid out again: another chapter, another style. */
  layoutVersion = 0;
  #env;
  #layout = null;
  /** The offset the next layout lands on; after it, where the page on screen starts. */
  #anchor;

  /** `start`, `{ chapter, offset }`, opens the book there rather than where it was left. */
  constructor(env, book, start = null) {
    super();
    this.#env = env;
    this.book = book;
    this.style = env.settings.style();
    this.chapter = start?.chapter ?? book.readingChapter;
    this.#anchor = start?.offset ?? book.readingOffset;
    this.start = start;
  }

  async load() {
    const { library } = this.#env;
    // The open book is searched first, then the rest of its group.
    const others = this.book.groupId ? (await library.inGroup(this.book.groupId)).filter((other) => other.id !== this.book.id) : [];
    this.corpus = new BookCorpus([this.book, ...others], library);
    try {
      const file = await library.file(this.book.id);
      if (!file) throw new Error("Its file is missing from this device.");
      this.document = await loadDocument(file);
    } catch (error) {
      this.status = "failed";
      this.error = error.message;
      this.changed();
      return;
    }
    this.corpus.provide(this.book.id, this.document.chapters.map((chapter) => chapter.text));
    if (this.document.language && this.document.language !== this.book.language) {
      this.book = { ...this.book, language: this.document.language };
      await library.saveLanguage(this.book.id, this.book.language);
    }
    this.chapter = Math.min(Math.max(this.chapter, 0), this.document.chapters.length - 1);
    // A place that came from another device is found in this text now that it is here.
    if (!this.start && this.book.placePending && this.book.place) this.#anchor = resolvePlace(this.book.place, this.chapterText);
    this.status = "loaded";
    this.#relayout();
  }

  get language() {
    return this.document?.language || this.book.language;
  }

  get chapterText() {
    return this.document?.chapters[this.chapter]?.text ?? "";
  }

  get pageStart() {
    return this.#layout ? this.#layout.startOf(this.page) : this.#anchor;
  }

  get pageEnd() {
    return this.#layout && this.page + 1 < this.pageCount ? this.#layout.startOf(this.page + 1) : this.chapterText.length;
  }

  /** The text of the page on screen, so a conversation can be about it. */
  get pageText() {
    return this.chapterText.slice(this.pageStart, this.pageEnd);
  }

  /** The end of the page on screen: how far the reader has got. */
  get position() {
    return { bookId: this.book.id, chapter: this.chapter, offset: this.pageEnd };
  }

  /** The corpus with the position: what lookups and conversations search. */
  get scope() {
    return { corpus: this.corpus, upTo: this.position };
  }

  /** The entry of the table of contents the page falls under, or -1. */
  get contentsEntry() {
    return this.document ? contentsEntryAt(this.document.contents, this.chapter, this.pageStart) : -1;
  }

  /** The view laid the chapter out: `{ count, startOf(page), pageOf(offset) }`. */
  laidOut(layout) {
    this.#layout = layout;
    this.pageCount = layout.count;
    this.#turnTo(layout.pageOf(this.#anchor));
  }

  setStyle(style) {
    this.style = style;
    this.#relayout();
  }

  next() {
    if (!this.#layout) return;
    if (this.page + 1 < this.pageCount) this.#turnTo(this.page + 1);
    else if (this.chapter + 1 < this.document.chapters.length) this.#showChapter(this.chapter + 1, 0);
  }

  previous() {
    if (!this.#layout) return;
    if (this.page > 0) this.#turnTo(this.page - 1);
    else if (this.chapter > 0) this.#showChapter(this.chapter - 1, Infinity);
  }

  /** Turns to the page holding `offset` of `chapter`. */
  goTo(chapter, offset) {
    if (!this.document || chapter < 0 || chapter >= this.document.chapters.length) return;
    if (chapter === this.chapter && this.#layout) this.#turnTo(this.#layout.pageOf(offset));
    else this.#showChapter(chapter, offset);
  }

  /** The word at `offset` of the chapter with its sentence, as a lookup takes it; null when it is not on a word. */
  wordAt(offset) {
    const selection = selectionAt(this.chapterText, offset, this.language);
    if (!selection) return null;
    return { ...selection, language: this.language, bookId: this.book.id };
  }

  #showChapter(chapter, anchor) {
    this.chapter = chapter;
    this.#anchor = anchor;
    this.#relayout();
  }

  #relayout() {
    this.#layout = null;
    this.layoutVersion++;
    this.changed();
  }

  #turnTo(page) {
    this.page = Math.min(Math.max(page, 0), Math.max(this.pageCount - 1, 0));
    this.#anchor = this.pageStart;
    this.#savePosition();
    this.changed();
  }

  #savePosition() {
    const offset = this.pageStart;
    const place = placeAt(this.chapter, this.chapterText, offset);
    this.book = { ...this.book, readingChapter: this.chapter, readingOffset: offset, place, placePending: false };
    this.#env.library.savePosition(this.book.id, this.chapter, offset, place).catch((error) => console.warn(error));
  }
}
