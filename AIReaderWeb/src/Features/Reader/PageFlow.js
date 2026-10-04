// A chapter laid out as pages by the browser itself: its paragraphs flow
// into CSS columns one page wide, and a page is shown by sliding the flow
// across a window that shows one column. Every text node remembers where it
// starts in the chapter's text, so a click, a page start and a word to mark
// are all read straight back into chapter offsets.
import { chapterParagraphs } from "../../Domain/Reading/ChapterParagraphs.js";
import { basePixelSize, fontFamily, pageSize } from "../../Domain/Reading/ReadingStyle.js";
import { h } from "../Common/Ui.js";

const tags = { bold: "b", italic: "i", superscript: "sup", subscript: "sub", heading: "strong" };

export class PageFlow {
  #pieces = [];
  #byNode = new Map();
  #urls = [];
  #stride = 1;
  #starts = new Map();

  constructor() {
    this.flow = h("div", { class: "flow" });
    this.window = h("div", { class: "page-window" }, this.flow);
  }

  /** Lays `chapter` out for a container of `width` × `height`; returns `{ count, startOf, pageOf }`. */
  async render(chapter, style, language, width, height) {
    this.dispose();
    const size = pageSize(style, width, height);
    const gap = style.margin * 2;
    this.#stride = size.width + gap;
    Object.assign(this.window.style, { width: `${size.width}px`, height: `${size.height}px`, marginTop: `${style.margin}px` });
    Object.assign(this.flow.style, {
      width: `${size.width}px`,
      height: `${size.height}px`,
      columnWidth: `${size.width}px`,
      columnGap: `${gap}px`,
      fontFamily: fontFamily(style),
      fontSize: `${basePixelSize * style.scale}px`,
      lineHeight: `calc(1.3em + ${style.lineSpacing}px)`,
      transform: "translateX(0)",
    });
    this.flow.lang = language || "";
    this.flow.style.setProperty("--picture-height", `${Math.floor(size.height * 0.92)}px`);
    this.flow.replaceChildren(...chapterParagraphs(chapter).map((paragraph) => this.#paragraph(chapter, paragraph)));
    // Pictures and faces change the layout as they arrive; measure after.
    await Promise.all([...this.flow.querySelectorAll("img")].map((image) => image.decode().catch(() => {})));
    await document.fonts?.ready;
    this.#starts.clear();
    const count = this.#pieces.length ? this.#column(chapter.text.length) + 1 : 1;
    return {
      count,
      pageOf: (offset) => Math.min(Math.max(this.#column(offset), 0), count - 1),
      startOf: (page) => this.#startOf(page, chapter.text.length),
    };
  }

  show(page) {
    this.flow.style.transform = `translateX(${-page * this.#stride}px)`;
  }

  #paragraph(chapter, paragraph) {
    if (paragraph.image !== undefined) {
      const blob = chapter.images[paragraph.image].blob;
      const url = blob ? URL.createObjectURL(blob) : "";
      if (url) this.#urls.push(url);
      const image = h("img", { src: url, alt: "" });
      this.#add({ start: paragraph.start, end: paragraph.end, node: image });
      return h("p", { class: "picture" }, image);
    }
    const element = h("p", { class: paragraph.heading ? "heading" : paragraph.end === paragraph.start ? "blank" : null });
    for (const run of paragraph.runs) {
      const text = document.createTextNode(chapter.text.slice(run.start, run.end));
      this.#add({ start: run.start, end: run.end, node: text });
      element.append(run.kinds.filter((kind) => !(paragraph.heading && kind === "heading"))
        .reduce((inner, kind) => h(tags[kind], {}, inner), text));
    }
    return element;
  }

  #add(piece) {
    this.#pieces.push(piece);
    this.#byNode.set(piece.node, piece);
  }

  /** The piece holding `offset`, or the first after it when the offset falls on a paragraph break. */
  #pieceAt(offset) {
    let low = 0;
    let high = this.#pieces.length - 1;
    while (low < high) {
      const middle = (low + high) >> 1;
      if (this.#pieces[middle].end <= offset) low = middle + 1;
      else high = middle;
    }
    return this.#pieces[low];
  }

  /** The column — the page — the character at `offset` sits in. */
  #column(offset) {
    const piece = this.#pieceAt(Math.max(offset, 0));
    if (!piece) return 0;
    let rect;
    if (piece.node.nodeType === Node.TEXT_NODE && piece.end > piece.start) {
      const index = Math.min(Math.max(offset - piece.start, 0), piece.end - piece.start - 1);
      const range = document.createRange();
      range.setStart(piece.node, index);
      range.setEnd(piece.node, index + 1);
      rect = range.getClientRects()[0] ?? range.getBoundingClientRect();
    } else {
      rect = (piece.node.parentElement ?? piece.node).getBoundingClientRect();
    }
    const left = this.flow.getBoundingClientRect().left;
    return Math.floor((rect.left - left + (this.#stride - parseFloat(this.flow.style.width)) / 2) / this.#stride);
  }

  /** The first offset on `page`: the smallest whose column is that page's. */
  #startOf(page, length) {
    if (page <= 0) return 0;
    if (!this.#starts.has(page)) {
      let low = 0;
      let high = length;
      while (low < high) {
        const middle = (low + high) >> 1;
        if (this.#column(middle) < page) low = middle + 1;
        else high = middle;
      }
      this.#starts.set(page, low);
    }
    return this.#starts.get(page);
  }

  /** The chapter offset of the character under a point, or null when the point is not on one. */
  offsetAt(x, y) {
    const position = document.caretPositionFromPoint?.(x, y);
    const range = position ? null : document.caretRangeFromPoint?.(x, y);
    const node = position?.offsetNode ?? range?.startContainer;
    const offset = position?.offset ?? range?.startOffset;
    const piece = this.#byNode.get(node);
    if (!piece || node.nodeType !== Node.TEXT_NODE) return null;
    // The caret sits between characters; only a point on one of them counts.
    const probe = document.createRange();
    for (const index of [offset, offset - 1]) {
      if (index < 0 || index >= node.length) continue;
      probe.setStart(node, index);
      probe.setEnd(node, index + 1);
      for (const rect of probe.getClientRects()) {
        if (x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom) return piece.start + index;
      }
    }
    return null;
  }

  /** A DOM range over `[start, end)` of the chapter, for marking a word. */
  range(start, end) {
    const at = (offset, isEnd) => {
      const piece = this.#pieces.find((candidate) => candidate.node.nodeType === Node.TEXT_NODE
        && (isEnd ? candidate.start < offset && offset <= candidate.end : candidate.start <= offset && offset < candidate.end));
      return piece ? [piece.node, offset - piece.start] : null;
    };
    const from = at(start, false);
    const to = at(end, true);
    if (!from || !to) return null;
    const range = document.createRange();
    range.setStart(...from);
    range.setEnd(...to);
    return range;
  }

  dispose() {
    for (const url of this.#urls) URL.revokeObjectURL(url);
    this.#urls = [];
    this.#pieces = [];
    this.#byNode.clear();
  }
}
