// Reads a page's content for its text: runs the text operators, places
// every glyph, and gathers the glyphs into lines, in the order the page
// draws them — `{ text, left, right, y, size }` in points, `y` the baseline's
// distance below the top of the page. Pictures and paths are passed over.
// The same reading as the Kindle app's `PdfPageText`.
import { PdfFont } from "./PdfFont.js";
import { isDict, isName, isStream, PdfLexer } from "./PdfObject.js";

const identity = [1, 0, 0, 1, 0, 0];

/** `m`, followed by `next`. */
function then(m, next) {
  return [
    m[0] * next[0] + m[1] * next[2], m[0] * next[1] + m[1] * next[3],
    m[2] * next[0] + m[3] * next[2], m[2] * next[1] + m[3] * next[3],
    m[4] * next[0] + m[5] * next[2] + next[4], m[4] * next[1] + m[5] * next[3] + next[5],
  ];
}

const translation = (x, y) => [1, 0, 0, 1, x, y];

function matrixOf(values) {
  if (!Array.isArray(values) || values.length < 6) return identity;
  return values.slice(-6).map((value) => (typeof value === "number" ? value : 0));
}

const numberAt = (operands, count, index) => {
  const value = operands.length >= count ? operands[operands.length - count + index] : 0;
  return typeof value === "number" ? value : 0;
};

class Reader {
  lines = [];
  #current = null;
  #state = { ctm: identity, font: null, fontSize: 0, charSpacing: 0, wordSpacing: 0, scale: 1, leading: 0, rise: 0 };
  #saved = [];
  #text = identity;
  #line = identity;

  constructor(document, top, fonts) {
    this.document = document;
    this.top = top;
    this.fonts = fonts;
  }

  /** Fonts are kept as promises, so a page that names one twice reads it once. */
  async #font(resources, name) {
    const dictionary = this.document.get(this.document.get(resources, "Font"), name);
    if (!isDict(dictionary)) return null;
    if (!this.fonts.has(dictionary)) this.fonts.set(dictionary, PdfFont.read(this.document, dictionary));
    return this.fonts.get(dictionary);
  }

  #nextLine(x, y) {
    this.#line = then(translation(x, y), this.#line);
    this.#text = this.#line;
  }

  async run(content, resources, depth) {
    const lexer = new PdfLexer(content);
    let operands = [];
    const state = () => this.#state;
    while (!lexer.atEnd()) {
      const token = lexer.next();
      if (token?.kind !== "op") {
        operands.push(token);
        continue;
      }
      const op = token.text;
      const shown = operands.at(-1)?.text ?? "";
      if (op === "BT") {
        this.#text = this.#line = identity;
      } else if (op === "Tj") {
        this.#show(shown);
      } else if (op === "TJ" && Array.isArray(operands.at(-1))) {
        for (const item of operands.at(-1)) {
          if (item?.kind === "string") this.#show(item.text);
          else if (typeof item === "number") this.#text = then(translation((-item / 1000) * state().fontSize * state().scale, 0), this.#text);
        }
      } else if (op === "'" || op === '"') {
        if (op === '"') {
          state().wordSpacing = numberAt(operands, 3, 0);
          state().charSpacing = numberAt(operands, 3, 1);
        }
        this.#nextLine(0, -state().leading);
        this.#show(shown);
      } else if (op === "Td" || op === "TD") {
        if (op === "TD") state().leading = -numberAt(operands, 2, 1);
        this.#nextLine(numberAt(operands, 2, 0), numberAt(operands, 2, 1));
      } else if (op === "T*") {
        this.#nextLine(0, -state().leading);
      } else if (op === "Tm") {
        this.#text = this.#line = matrixOf(operands);
      } else if (op === "Tf" && operands.length >= 2) {
        state().font = await this.#font(resources, operands.at(-2)?.text ?? "");
        state().fontSize = numberAt(operands, 1, 0);
      } else if (op === "Tc") {
        state().charSpacing = numberAt(operands, 1, 0);
      } else if (op === "Tw") {
        state().wordSpacing = numberAt(operands, 1, 0);
      } else if (op === "Tz") {
        state().scale = numberAt(operands, 1, 0) / 100;
      } else if (op === "TL") {
        state().leading = numberAt(operands, 1, 0);
      } else if (op === "Ts") {
        state().rise = numberAt(operands, 1, 0);
      } else if (op === "q") {
        if (this.#saved.length < 64) this.#saved.push({ ...state() });
      } else if (op === "Q") {
        if (this.#saved.length) this.#state = this.#saved.pop();
      } else if (op === "cm") {
        state().ctm = then(matrixOf(operands), state().ctm);
      } else if (op === "Do" && operands.length && depth < 8) {
        // A form is a content stream of its own, drawn where it is placed.
        const form = this.document.get(this.document.get(resources, "XObject"), operands.at(-1)?.text ?? "");
        if (isStream(form) && isName(this.document.get(form, "Subtype"), "Form")) {
          const before = { ...state() };
          const depthBefore = this.#saved.length;
          const [text, line] = [this.#text, this.#line];
          state().ctm = then(matrixOf(this.document.get(form, "Matrix")), state().ctm);
          const own = this.document.get(form, "Resources");
          await this.run(await this.document.contents(form), isDict(own) ? own : resources, depth + 1);
          this.#state = before;
          this.#saved.length = Math.min(this.#saved.length, depthBefore);
          [this.#text, this.#line] = [text, line];
        }
      } else if (op === "BI") {
        while (!lexer.atEnd()) {
          const word = lexer.next();
          if (word?.kind === "op" && word.text === "ID") break;
        }
        lexer.skipInlineImage();
      }
      operands = [];
    }
  }

  #show(bytes) {
    const state = this.#state;
    const font = state.font ?? Reader.missing;
    for (const glyph of font.glyphs(bytes)) {
      const placed = then(this.#text, state.ctm);
      const origin = then(translation(0, state.rise), placed);
      const size = Math.abs(state.fontSize) * Math.hypot(placed[2], placed[3]);
      const advance = ((glyph.width / 1000) * state.fontSize + state.charSpacing + (glyph.isSpace ? state.wordSpacing : 0)) * state.scale;
      this.#place(glyph.text, origin[4], origin[5], size, advance * Math.hypot(placed[0], placed[1]));
      this.#text = then(translation(advance, 0), this.#text);
    }
  }

  #place(text, x, y, size, advance) {
    const down = this.top - y;
    const blank = !/\S/.test(text);
    const current = this.#current;
    if (current) {
      const em = Math.max(current.size, size);
      // The same baseline, give or take a superscript, and not far back.
      if (Math.abs(down - current.y) <= em * 0.5 && x >= current.right - em * 1.5) {
        // A no-break space is a space already.
        const spaced = current.text.endsWith(" ") || current.text.endsWith("\u00a0");
        if (x - current.right > size * 0.15 && !spaced && !blank) current.text += " ";
        if (!(blank && spaced)) current.text += text;
        current.right = Math.max(current.right, x + advance);
        current.size = Math.max(current.size, size);
        return;
      }
      this.flush();
    }
    if (!blank) this.#current = { text, left: x, right: x + advance, y: down, size };
  }

  flush() {
    const current = this.#current;
    this.#current = null;
    if (!current) return;
    current.text = current.text.replace(/ +$/, "");
    if (current.text) this.lines.push(current);
  }
}

/** A font the page names but does not define. */
Reader.missing = new PdfFont();

/**
 * The lines of one page. `fonts`, a Map the caller keeps across a book's
 * pages, holds each font as it is read, so every font is read once.
 */
export async function pageLines(document, page, fonts) {
  const contents = document.get(page.dictionary, "Contents");
  const parts = Array.isArray(contents) ? contents.map((part) => document.resolve(part)) : [contents];
  const pieces = [];
  for (const part of parts) pieces.push(await document.contents(part), Uint8Array.of(0x0a));
  const content = new Uint8Array(pieces.reduce((total, piece) => total + piece.length, 0));
  let offset = 0;
  for (const piece of pieces) {
    content.set(piece, offset);
    offset += piece.length;
  }
  const reader = new Reader(document, page.top, fonts);
  await reader.run(content, page.resources, 0);
  reader.flush();
  return reader.lines;
}
