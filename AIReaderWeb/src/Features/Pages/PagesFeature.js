// A PDF book's pages as the PDF draws them — a figure, a table, the page as
// printed — with a tapped word explained as on the reflowed text. Opened
// from the reader of a book made from a PDF on this device, at the page as
// far through the PDF as the reader is through the book, or where the page
// view was last left. The page's text runs together as a photographed
// page's does (`PagePassage`), so the sentence around a word is found
// across line breaks. Every word a tap would find can be marked on the page,
// as the reader chooses. The same as the iOS app's `PDFPagesFeature`; the
// zoom is the web's own, PDFKit keeping its own on iOS.
import { itemLines, pagePassage } from "../../Domain/Books/PagePassage.js";
import { selectionAt, wordsIn } from "../../Domain/Reading/WordContext.js";
import { Feature } from "../Common/Feature.js";

/** `zoom` kept from the width of the screen to four times it. */
export const clampedZoom = (zoom) => Math.min(Math.max(zoom, 1), 4);

export class PagesFeature extends Feature {
  status = "loading";
  error = "";
  page = 0;
  pageCount = 0;
  /** The page on screen: PDF.js's page and its text content, for the view to draw. */
  current = null;
  /** Whether every word a tap finds is marked on the page; kept between openings. */
  showsWords;
  /** How much larger than the width of the screen the page is drawn. */
  zoom = 1;
  #env;
  #reader;
  #open;
  #pdf = null;
  #text = null;

  /** `reader` is the reader feature it was opened from; `open` turns the PDF's bytes into a PDF.js document. */
  constructor(env, reader, open) {
    super();
    this.#env = env;
    this.#reader = reader;
    this.#open = open;
    this.showsWords = env.settings.pageWords();
  }

  async load() {
    try {
      const original = await this.#env.library.original(this.#reader.book.id);
      if (!original) throw new Error("This book’s PDF is not on this device.");
      this.#pdf = await this.#open(new Uint8Array(await original.arrayBuffer()));
      this.pageCount = this.#pdf.numPages;
      const start = this.#reader.pdfPage ?? Math.floor(this.#reader.fraction * this.pageCount);
      await this.#show(start);
      this.status = "loaded";
    } catch (error) {
      this.status = "failed";
      this.error = error.message;
    }
    this.changed();
  }

  next() {
    if (this.page + 1 < this.pageCount) this.#show(this.page + 1).then(() => this.changed());
  }

  previous() {
    if (this.page > 0) this.#show(this.page - 1).then(() => this.changed());
  }

  toggleWords() {
    this.showsWords = !this.showsWords;
    this.#env.settings.savePageWords(this.showsWords);
    this.changed();
  }

  /** Zooms to `zoom`, kept within `clampedZoom`. */
  zoomTo(zoom) {
    const clamped = clampedZoom(zoom);
    if (clamped === this.zoom) return;
    this.zoom = clamped;
    this.changed();
  }

  /**
   * The word at character `offset` of text item `item` on the page, with its
   * sentence, as a lookup takes it — and the stretch of the item it covers,
   * `{ from, to }`, to mark it; null when the offset is not on a word.
   */
  wordAt(item, offset) {
    const place = this.#text?.places[item];
    if (!place) return null;
    const { passage } = this.#text;
    const itemStart = passage.lineStarts[place.line] + place.column;
    const language = this.#reader.language;
    const selection = selectionAt(passage.text, itemStart + offset, language);
    if (!selection) return null;
    const length = this.current.content.items[item].str.length;
    const clamp = (at) => Math.min(Math.max(at - itemStart, 0), length);
    return {
      word: { ...selection, language, bookId: this.#reader.book.id },
      marked: { from: clamp(selection.start), to: clamp(selection.end) },
    };
  }

  /** Every word on the page a tap would find, as the stretches of text items it covers: `[{ item, from, to }]`. */
  words() {
    if (!this.#text) return [];
    const { passage, places } = this.#text;
    const words = wordsIn(passage.text, this.#reader.language);
    const marks = [];
    let first = 0;
    places.forEach((place, item) => {
      const start = passage.lineStarts[place.line] + place.column;
      const end = start + this.current.content.items[item].str.length;
      // Items run in the passage's order, so a word ended before one item has ended before the next.
      while (first < words.length && words[first].end <= start) first += 1;
      for (let index = first; index < words.length && words[index].start < end; index += 1) {
        marks.push({ item, from: Math.max(words[index].start, start) - start, to: Math.min(words[index].end, end) - start });
      }
    });
    return marks;
  }

  async #show(index) {
    const number = Math.min(Math.max(index, 0), this.pageCount - 1);
    const page = await this.#pdf.getPage(number + 1);
    const content = await page.getTextContent();
    const { lines, places } = itemLines(content.items);
    this.#text = { passage: pagePassage(lines), places };
    this.current = { page, content };
    this.page = number;
    // Left here, the page view reopens here while the book stays open.
    this.#reader.pdfPage = number;
  }
}
