// A chapter reduced to what a page shows: its prose with paragraphs on
// lines of their own, the few styles worth keeping, its pictures and its
// anchors. The same rules as the Kindle app's `Domain/Books/HtmlText.cpp`,
// so chapters are kept, dropped and read the same way on every device.
// Offsets are UTF-16 units, as on iOS.
import { scan } from "../../Support/XmlScanner.js";

/** Stands in for an illustration, on a line of its own. */
export const imagePlaceholder = "￼";

const blocks = new Set([
  "p", "div", "h1", "h2", "h3", "h4", "h5", "h6", "li", "ul", "ol", "tr", "table",
  "blockquote", "section", "article", "header", "footer", "aside", "hr", "pre",
  "dd", "dt", "dl", "figure", "figcaption", "body", "nav", "address",
]);
const skipped = new Set(["head", "script", "style", "title", "svg"]);
const kinds = new Map([
  ...["h1", "h2", "h3", "h4", "h5", "h6"].map((name) => [name, "heading"]),
  ["b", "bold"], ["strong", "bold"],
  ["i", "italic"], ["em", "italic"], ["cite", "italic"], ["dfn", "italic"],
  ["sup", "superscript"], ["sub", "subscript"],
]);

/** The text of a footnote reference: a number or a sign such as * or †, perhaps in brackets. */
function isNoteMark(text) {
  const characters = [...text];
  if (!characters.length || characters.length > 5) return false;
  return characters.every((c) => /\p{Nd}/u.test(c) || "*†‡§↩[]()".includes(c));
}

/** What an element is for, as EPUB 3 says it: its epub:type and ARIA role. */
function purpose(attributes) {
  return `${attributes.type ?? ""} ${attributes.role ?? ""}`;
}

/** `{ text, spans: [{ start, end, kind }], images: [{ offset, source }], anchors: Map(id → offset) }` */
export function plainText(markup) {
  const out = { text: "", spans: [], images: [], anchors: new Map() };
  let skipName = "";
  let skipDepth = 0;
  let preDepth = 0;
  let pendingSpace = false;
  const openSpans = [];
  let link = null;

  const endsLine = () => !out.text || out.text.endsWith("\n");
  const breakParagraph = () => {
    pendingSpace = false;
    if (!endsLine()) out.text += "\n";
  };
  const openSpan = (name, kind) => {
    // A space owed before the span belongs outside it.
    if (pendingSpace) out.text += " ";
    pendingSpace = false;
    openSpans.push({ name, start: out.text.length, kind });
  };
  const closeSpan = (name) => {
    for (let i = openSpans.length - 1; i >= 0; i--) {
      if (openSpans[i].name !== name) continue;
      const { start, kind } = openSpans[i];
      if (out.text.length > start) out.spans.push({ start, end: out.text.length, kind });
      openSpans.splice(i, 1);
      return;
    }
  };
  const image = (source) => {
    if (!source) return;
    breakParagraph();
    out.images.push({ offset: out.text.length, source });
    out.text += imagePlaceholder;
    breakParagraph();
  };
  const text = (chunk) => {
    if (skipDepth) return;
    if (preDepth) {
      out.text += chunk;
      return;
    }
    for (const c of chunk) {
      if (c === " " || c === "\t" || c === "\r" || c === "\n") {
        // Whitespace collapses, and never opens a paragraph.
        if (!endsLine()) pendingSpace = true;
        continue;
      }
      // A soft hyphen would sit inside the word when it is looked up.
      if (c === "­") continue;
      if (pendingSpace) out.text += " ";
      pendingSpace = false;
      out.text += c;
    }
  };
  // A footnote reference is dropped: the reader cannot follow it, and a
  // number glued to a word spoils looking the word up.
  const closeLink = () => {
    if (!link) return;
    const closed = link;
    link = null;
    const looksLike = closed.mayBeNote && isNoteMark(out.text.slice(closed.start).trim());
    if (!closed.declared && !looksLike) return;
    out.text = out.text.slice(0, closed.start);
    pendingSpace = closed.pendingSpace;
    while (out.spans.length && out.spans.at(-1).start >= closed.start) out.spans.pop();
    for (const open of openSpans) open.start = Math.min(open.start, closed.start);
  };

  scan(markup, {
    onStart(name, attributes) {
      // Pictures come as <img src> and, inside an <svg>, as <image href>.
      if (name === "img" && !skipDepth) return image(attributes.src);
      if (name === "image") return image(attributes.href);
      if (skipDepth) {
        if (name === skipName) skipDepth++;
        return;
      }
      if (skipped.has(name) || "hidden" in attributes || purpose(attributes).includes("pagebreak")) {
        skipName = name;
        skipDepth = 1;
        return;
      }
      if (name === "br") {
        out.text += "\n";
        pendingSpace = false;
        return;
      }
      // Cells of one row stay on one line, a space apart.
      if (name === "td" || name === "th") {
        if (!endsLine()) pendingSpace = true;
        return;
      }
      if (name === "pre") preDepth++;
      if (blocks.has(name)) breakParagraph();
      // Recorded after the break, so the anchor is the paragraph's start.
      const id = attributes.id || (name === "a" ? attributes.name : "");
      if (id && !out.anchors.has(id)) out.anchors.set(id, out.text.length);
      // A rule is a scene break: a blank line, since paragraphs are only indented.
      if (name === "hr" && out.text) out.text += "\n";
      if (name === "a") {
        // A mark at the head of a paragraph is a note's own number, not a reference to one.
        link = {
          start: out.text.length,
          pendingSpace,
          declared: purpose(attributes).includes("noteref"),
          mayBeNote: !endsLine() && (attributes.href ?? "").includes("#"),
        };
      }
      if (name === "q") text("“");
      if (kinds.has(name)) openSpan(name, kinds.get(name));
    },
    onEnd(name) {
      if (skipDepth) {
        if (name === skipName) skipDepth--;
        return;
      }
      if (name === "pre" && preDepth) preDepth--;
      if (name === "q") {
        pendingSpace = false;
        out.text += "”";
      }
      if (kinds.has(name)) closeSpan(name);
      if (name === "a") closeLink();
      if (blocks.has(name)) breakParagraph();
    },
    onText: text,
  });
  while (openSpans.length) closeSpan(openSpans.at(-1).name);
  out.text = out.text.replace(/\n+$/, "");
  return out;
}
