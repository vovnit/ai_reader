// A PDF book's pages as the PDF draws them. PDF.js draws the page and lays
// its text over it, transparent, where each word is printed, so a tap finds
// the word under it the way a tap on the reflowed text does; the word looked
// up is marked on the page while its lookup is open. Turned with the
// buttons, the arrow keys or a swipe.
import { openPdf, pdfjs } from "../../Services/PdfPages.js";
import { Screen } from "../Common/Screen.js";
import { h, note } from "../Common/Ui.js";
import { LookupView } from "../Lookup/LookupView.js";
import { PagesFeature } from "./PagesFeature.js";

const typing = (target) => target.closest?.("input, textarea, select, [contenteditable], dialog");

export class PagesView extends Screen {
  #drawn = null;
  #task = null;
  /** PDF.js's text layer on screen, its spans one for each of the page's text items. */
  #textLayer = null;
  /** The word looked up, `{ current, item, from, to }`, marked again whenever the page is drawn again. */
  #marked = null;
  #swiped = false;

  /** `reader` is the reader screen it was opened from. */
  constructor(env, navigator, reader) {
    super(navigator, "Pages", { placement: "main" });
    this.env = env;
    this.reader = reader;
    this.backName = "Pages";
    this.feature = new PagesFeature(env, reader.feature, openPdf);
    this.previousButton = this.addAction("Previous", () => this.feature.previous(), { title: "Previous page" });
    this.nextButton = this.addAction("Next", () => this.feature.next(), { title: "Next page" });
    this.canvas = h("canvas", {});
    this.text = h("div", { class: "textLayer" });
    this.sheet = h("div", { class: "pdf-page" }, this.canvas, this.text);
    this.status = h("div", {});
    this.footer = h("footer", { class: "reader-footer" });
    this.setBody(this.status, this.sheet, this.footer);
    this.#listen();
    this.onClose(() => {
      this.#task?.cancel();
      CSS.highlights?.delete("page-word");
    });
    this.watch(this.feature, () => this.#render());
    this.feature.load();
  }

  shown() {
    this.#marked = null;
    this.#mark();
  }

  #mark() {
    const marked = this.#marked?.current === this.#drawn ? this.#marked : null;
    const node = marked && this.#textLayer?.textDivs[marked.item]?.firstChild;
    if (!node || !globalThis.Highlight) return CSS.highlights?.delete("page-word");
    const range = document.createRange();
    range.setStart(node, marked.from);
    range.setEnd(node, marked.to);
    CSS.highlights.set("page-word", new Highlight(range));
  }

  #listen() {
    this.sheet.addEventListener("click", (event) => this.#tap(event));
    const keys = (event) => {
      if (!this.element.isConnected || typing(event.target) || event.altKey || event.metaKey || event.ctrlKey) return;
      if (["ArrowRight", "PageDown"].includes(event.key)) this.feature.next();
      else if (["ArrowLeft", "PageUp"].includes(event.key)) this.feature.previous();
    };
    document.addEventListener("keydown", keys);
    this.onClose(() => document.removeEventListener("keydown", keys));

    let start = null;
    this.sheet.addEventListener("pointerdown", (event) => {
      start = event.pointerType === "mouse" ? null : { x: event.clientX, y: event.clientY };
    });
    this.sheet.addEventListener("pointerup", (event) => {
      if (!start) return;
      const dx = event.clientX - start.x;
      const dy = event.clientY - start.y;
      start = null;
      if (Math.abs(dx) < 40 || Math.abs(dx) < Math.abs(dy)) return;
      this.#swiped = true;
      if (dx < 0) this.feature.next();
      else this.feature.previous();
    });

    // A lookup opening beside the page narrows it too.
    let width = 0;
    const resize = new ResizeObserver(() => {
      if (this.body.clientWidth === width) return;
      width = this.body.clientWidth;
      this.#drawn = null;
      this.#render();
    });
    resize.observe(this.body);
    this.onClose(() => resize.disconnect());
  }

  #render() {
    const { status, error, page, pageCount, current } = this.feature;
    this.status.replaceChildren(...[status === "loading" ? note("Opening the PDF…") : status === "failed" ? h("p", { class: "error message" }, `Couldn’t open the pages.\n${error}`) : null].filter(Boolean));
    this.sheet.hidden = status !== "loaded";
    this.previousButton.disabled = page === 0;
    this.nextButton.disabled = page + 1 >= pageCount;
    this.footer.textContent = status === "loaded" ? `Page ${page + 1} of ${pageCount}` : "";
    if (status === "loaded" && current !== this.#drawn) this.#draw(current);
  }

  /** Draws the page the width of the screen, sharp on a dense display, with its text laid over it. */
  async #draw(current) {
    const style = getComputedStyle(this.body);
    const width = this.body.clientWidth - parseFloat(style.paddingLeft) - parseFloat(style.paddingRight);
    if (width <= 0) return;
    this.#drawn = current;
    this.#task?.cancel();
    const { page, content } = current;
    const viewport = page.getViewport({ scale: width / page.getViewport({ scale: 1 }).width });
    const ratio = globalThis.devicePixelRatio || 1;
    this.canvas.width = Math.floor(viewport.width * ratio);
    this.canvas.height = Math.floor(viewport.height * ratio);
    this.sheet.style.width = `${viewport.width}px`;
    this.sheet.style.height = `${viewport.height}px`;
    this.#task = page.render({ canvas: this.canvas, viewport, transform: ratio === 1 ? null : [ratio, 0, 0, ratio, 0, 0] });
    try {
      await this.#task.promise;
    } catch (error) {
      // Another page was asked for while this one was drawn.
      if (error?.name === "RenderingCancelledException") return;
      throw error;
    }
    if (this.#drawn !== current) return;
    const { TextLayer } = await pdfjs();
    const text = h("div", { class: "textLayer" });
    text.style.setProperty("--total-scale-factor", String(viewport.scale));
    const layer = new TextLayer({ textContentSource: content, container: text, viewport });
    await layer.render();
    if (this.#drawn !== current) return;
    this.text.replaceWith(text);
    this.text = text;
    this.#textLayer = layer;
    this.#mark();
  }

  #tap(event) {
    if (this.#swiped) {
      this.#swiped = false;
      return;
    }
    if (!document.getSelection().isCollapsed) return;
    const caret = caretAt(event.clientX, event.clientY);
    const span = caret?.node.parentElement;
    // The browser finds the nearest text even far from the tap; only text under it counts.
    const box = span?.getBoundingClientRect();
    const under = box && event.clientX >= box.left && event.clientX <= box.right && event.clientY >= box.top && event.clientY <= box.bottom;
    const item = under ? this.#textLayer?.textDivs.indexOf(span) ?? -1 : -1;
    // A tap between words, on a picture or in the margin finds nothing.
    const found = item < 0 ? null : this.feature.wordAt(item, caret.offset);
    if (!found) return;
    // Marked after the lookup opens: coming back to this screen clears the mark.
    this.navigator.popTo(this);
    this.navigator.push(new LookupView(this.env, this.navigator, found.word, this.reader.link));
    this.#marked = { current: this.#drawn, item, ...found.marked };
    this.#mark();
  }
}

/** The text node and offset under a point, in either browser's way of asking. */
function caretAt(x, y) {
  if (document.caretPositionFromPoint) {
    const position = document.caretPositionFromPoint(x, y);
    return position?.offsetNode?.nodeType === Node.TEXT_NODE ? { node: position.offsetNode, offset: position.offset } : null;
  }
  const range = document.caretRangeFromPoint?.(x, y);
  return range?.startContainer?.nodeType === Node.TEXT_NODE ? { node: range.startContainer, offset: range.startOffset } : null;
}
