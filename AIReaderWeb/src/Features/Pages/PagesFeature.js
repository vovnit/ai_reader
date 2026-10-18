// A PDF book's pages as the PDF draws them — a figure, a table, the page as
// printed — with a tapped word explained as on the reflowed text. Opened
// from the reader of a book made from a PDF on this device, at the page as
// far through the PDF as the reader is through the book, or where the page
// view was last left. The page's text runs together as a photographed
// page's does (`PagePassage`), so the sentence around a word is found
// across line breaks. The same as the iOS app's `PDFPagesFeature`.
import { itemLines, pagePassage } from "../../Domain/Books/PagePassage.js";
import { selectionAt } from "../../Domain/Reading/WordContext.js";
import { Feature } from "../Common/Feature.js";

export class PagesFeature extends Feature {
  status = "loading";
  error = "";
  page = 0;
  pageCount = 0;
  /** The page on screen: PDF.js's page and its text content, for the view to draw. */
  current = null;
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
