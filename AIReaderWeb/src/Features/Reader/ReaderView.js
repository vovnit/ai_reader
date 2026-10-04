// The book as pages. Click a word to look it up, blank space to turn the
// page — or the arrow keys, Page Up and Down, the space bar, the wheel, a
// swipe. The menu holds the rest.
import { Screen } from "../Common/Screen.js";
import { h, note } from "../Common/Ui.js";
import { LookupView } from "../Lookup/LookupView.js";
import { MenuView } from "../Menu/MenuView.js";
import { SearchView } from "../Search/SearchView.js";
import { PageFlow } from "./PageFlow.js";
import { ReaderFeature } from "./ReaderFeature.js";

const typing = (target) => target.closest?.("input, textarea, select, [contenteditable], dialog, #panel");

export class ReaderView extends Screen {
  #rendered = "";
  #rendering = false;
  #swiped = false;
  #lastTurn = 0;
  /** The word being looked up, marked on the page while its lookup is open. */
  #marked = null;

  /** `onClosed` runs when the book is closed — a sync, on the shelf's behalf. `start` opens it at `{ chapter, offset }`. */
  constructor(env, navigator, book, onClosed, start = null) {
    super(navigator, book.title, { placement: "main" });
    this.env = env;
    this.onClosed = onClosed;
    this.backName = "Book";
    this.feature = new ReaderFeature(env, book, start);
    this.pages = new PageFlow();
    this.area = h("div", { class: "reader-area" }, this.pages.window);
    this.footer = h("footer", { class: "reader-footer" });
    this.status = h("div", { class: "reader-status" });
    this.body.classList.add("reader");
    this.setBody(this.status, this.area, this.footer);
    this.addAction("Menu", () => this.openMenu());
    this.#listen();
    this.onClose(() => {
      this.pages.dispose();
      onClosed?.();
    });
    this.watch(this.feature, () => this.#update());
    this.feature.load();
  }

  /** How searches and X-rays send the reader to a place: here, or in another book of the group. */
  get link() {
    return {
      scope: this.feature.scope,
      goTo: (hit) => {
        this.navigator.popTo(this);
        if (hit.bookId === this.feature.book.id) return this.feature.goTo(hit.chapter, hit.offset);
        this.env.library.find(hit.bookId).then((book) => {
          if (!book) return;
          this.navigator.pop(this);
          this.navigator.push(new ReaderView(this.env, this.navigator, book, this.onClosed, { chapter: hit.chapter, offset: hit.offset }));
        });
      },
    };
  }

  shown() {
    this.#marked = null;
    this.#mark();
  }

  #mark() {
    const range = this.#marked && this.pages.range(this.#marked.start, this.#marked.end);
    if (range && globalThis.Highlight) CSS.highlights.set("lookup", new Highlight(range));
    else CSS.highlights?.delete("lookup");
  }

  openMenu() {
    this.navigator.popTo(this);
    this.navigator.push(new MenuView(this.env, this.navigator, this));
  }

  #listen() {
    this.area.addEventListener("click", (event) => this.#tap(event));
    const keys = (event) => {
      if (!this.element.isConnected || typing(event.target) || event.altKey) return;
      const command = event.metaKey || event.ctrlKey;
      if (command && event.key === "f") {
        event.preventDefault();
        this.navigator.popTo(this);
        this.navigator.push(new SearchView(this.navigator, this.link, this.feature));
      } else if (command) {
        return;
      } else if (["ArrowRight", "ArrowDown", "PageDown"].includes(event.key) || (event.key === " " && !event.shiftKey)) {
        event.preventDefault();
        this.feature.next();
      } else if (["ArrowLeft", "ArrowUp", "PageUp"].includes(event.key) || (event.key === " " && event.shiftKey)) {
        event.preventDefault();
        this.feature.previous();
      }
    };
    document.addEventListener("keydown", keys);
    this.onClose(() => document.removeEventListener("keydown", keys));

    this.area.addEventListener("wheel", (event) => {
      event.preventDefault();
      const delta = Math.abs(event.deltaY) > Math.abs(event.deltaX) ? event.deltaY : event.deltaX;
      // A trackpad sends a stream of small deltas for one gesture: one turn each.
      if (Math.abs(delta) < 8 || performance.now() - this.#lastTurn < 400) return;
      this.#lastTurn = performance.now();
      if (delta > 0) this.feature.next();
      else this.feature.previous();
    }, { passive: false });

    let start = null;
    this.area.addEventListener("pointerdown", (event) => {
      start = event.pointerType === "mouse" ? null : { x: event.clientX, y: event.clientY };
    });
    this.area.addEventListener("pointerup", (event) => {
      if (!start) return;
      const dx = event.clientX - start.x;
      const dy = event.clientY - start.y;
      start = null;
      if (Math.abs(dx) < 40 || Math.abs(dx) < Math.abs(dy)) return;
      this.#swiped = true;
      if (dx < 0) this.feature.next();
      else this.feature.previous();
    });

    const resize = new ResizeObserver(() => this.#update());
    resize.observe(this.area);
    this.onClose(() => resize.disconnect());
  }

  #tap(event) {
    if (this.#swiped) {
      this.#swiped = false;
      return;
    }
    if (this.feature.status !== "loaded" || !document.getSelection().isCollapsed) return;
    const offset = this.pages.offsetAt(event.clientX, event.clientY);
    const word = offset === null ? null : this.feature.wordAt(offset);
    if (word) return this.#lookUp(word);
    const bounds = this.area.getBoundingClientRect();
    if (event.clientX < bounds.left + bounds.width / 2) this.feature.previous();
    else this.feature.next();
  }

  #lookUp(word) {
    this.navigator.popTo(this);
    this.navigator.push(new LookupView(this.env, this.navigator, word, this.link));
    this.#marked = { start: word.start, end: word.end };
    this.#mark();
  }

  async #update() {
    const { status, error } = this.feature;
    this.status.replaceChildren(...[status === "loading" ? note("Opening…") : status === "failed" ? h("p", { class: "error message" }, `Couldn’t open the book.\n${error}`) : null].filter(Boolean));
    if (status !== "loaded" || this.#rendering) return;
    const key = () => `${this.feature.chapter}|${this.feature.layoutVersion}|${this.area.clientWidth}x${this.area.clientHeight}`;
    const wanted = key();
    if (wanted !== this.#rendered && this.area.clientWidth > 0 && this.area.clientHeight > 0) {
      this.#rendering = true;
      const chapter = this.feature.document.chapters[this.feature.chapter];
      const layout = await this.pages.render(chapter, this.feature.style, this.feature.language, this.area.clientWidth, this.area.clientHeight);
      this.#rendering = false;
      // Turned to another chapter, or resized, while this one was laid out: again.
      if (key() !== wanted) return this.#update();
      this.#rendered = wanted;
      // The page's text was made anew; the mark goes onto the new one.
      this.#mark();
      this.feature.laidOut(layout);
      return;
    }
    this.pages.show(this.feature.page);
    const chapters = this.feature.document.chapters.length;
    this.footer.textContent = `Chapter ${this.feature.chapter + 1}/${chapters}  ·  Page ${this.feature.page + 1}/${this.feature.pageCount}`;
  }
}
