// Reads an HTML book, already decoded, into `{ title, author, language,
// chapters }`: each block element's text is a paragraph, a heading's a
// heading, and `<h1>`, `<h2>` and `<h3>` open chapters (Gutenberg often sets
// parts in `<h2>` and their chapters in `<h3>`). Tolerant of what real files
// carry: unclosed paragraphs, stray end tags, upper-case names, no `<html>`
// or `<body>`. The same as `HtmlBook.cpp` (Kindle) and `HTMLBook.swift`
// (iOS).
import { scan } from "../../Support/XmlScanner.js";
import { gutenbergParts } from "./Gutenberg.js";
import { collapsed, isBlank, textChapters } from "./TextChapters.js";

// Elements that end the paragraph before them and begin a new one.
const blocks = new Set([
  "p", "div", "li", "dt", "dd", "blockquote", "pre", "td", "th", "caption", "figcaption", "figure",
  "section", "article", "header", "footer", "aside", "address", "main", "center", "hr",
  "h1", "h2", "h3", "h4", "h5", "h6", "tr", "table", "ul", "ol", "dl", "body",
]);
// Elements whose text is not the book's.
const hidden = new Set(["script", "style", "noscript", "template", "svg", "math", "nav"]);
const headings = new Set(["h1", "h2", "h3", "h4", "h5", "h6"]);
// Elements a <head> holds; any other ends it, closed or not, as in a browser.
const headElements = new Set(["title", "meta", "link", "style", "script", "base", "noscript", "template"]);
// Elements whose content is not markup, so the scanner never sees it.
const rawTextElements = ["script", "style"];

const endsName = (c) => c === " " || c === "\t" || c === "\r" || c === "\n" || c === "/" || c === ">";

/** `markup` without what lies inside `<script>` and `<style>`, where a "<" would read as a tag that swallows the closing one, and the book after it. */
function withoutRawText(markup) {
  const lower = markup.replace(/[A-Z]/g, (c) => c.toLowerCase());
  let out = "";
  let kept = 0;
  let i = 0;
  while ((i = lower.indexOf("<", i)) >= 0) {
    if (lower.startsWith("<!--", i)) {
      const end = lower.indexOf("-->", i + 4);
      i = end < 0 ? lower.length : end + 3;
      continue;
    }
    const name = rawTextElements.find((element) => lower.startsWith(element, i + 1) && endsName(lower[i + 1 + element.length]));
    const open = name ? lower.indexOf(">", i) : -1;
    if (open < 0 || lower[open - 1] === "/") {
      i++;
      continue;
    }
    const close = lower.indexOf(`</${name}`, open);
    out += markup.slice(kept, open + 1);
    kept = close < 0 ? lower.length : close;
    i = kept;
  }
  return out + markup.slice(kept);
}

export function htmlBook(text, fallbackTitle) {
  let title = "";
  let firstH1 = "";
  let author = "";
  let language = "";
  let paragraphs = [];
  let gathered = "";
  let heading = "";
  let inHead = false;
  let inTitle = false;
  let titleText = "";
  // The hidden element being skipped, and how deep in ones of its name.
  let skipping = "";
  let depth = 0;

  const flush = () => {
    const paragraph = collapsed(gathered);
    gathered = "";
    if (isBlank(paragraph)) return;
    if (heading === "h1" && !firstH1) firstH1 = paragraph;
    paragraphs.push({ text: paragraph, heading: heading !== "", opens: heading === "h1" || heading === "h2" || heading === "h3" });
  };

  scan(withoutRawText(text), {
    onStart(name, attributes) {
      if (skipping) {
        if (name === skipping) depth++;
        return;
      }
      if (!headElements.has(name)) inHead = false;
      if (hidden.has(name)) {
        skipping = name;
        depth = 1;
      } else if (name === "html") {
        language = collapsed(attributes.lang ?? "").toLowerCase();
      } else if (name === "head") {
        inHead = true;
      } else if (name === "title") {
        inTitle = true;
        titleText = "";
      } else if (name === "meta" && collapsed(attributes.name ?? "").toLowerCase() === "author" && !author) {
        author = collapsed(attributes.content ?? "");
      } else if (name === "br") {
        // In a heading a break only sets the words on two lines: "CHAPTER I.<br>Down the Rabbit-Hole" is one title.
        if (heading) gathered += " ";
        else flush();
      } else if (blocks.has(name)) {
        flush();
        if (headings.has(name)) heading = name;
      }
    },
    onEnd(name) {
      if (skipping) {
        if (name === skipping && --depth === 0) skipping = "";
        return;
      }
      if (name === "head") {
        inHead = false;
      } else if (name === "title" && inTitle) {
        inTitle = false;
        if (!title) title = collapsed(titleText);
      } else if (blocks.has(name)) {
        flush();
        if (headings.has(name)) heading = "";
      }
    },
    onText(run) {
      if (skipping) return;
      if (inTitle) titleText += run;
      else if (!inHead) gathered += run;
    },
  });
  flush();

  const gutenberg = gutenbergParts(paragraphs.map((paragraph) => paragraph.text));
  if (gutenberg) paragraphs = paragraphs.slice(gutenberg.start + 1, gutenberg.end);
  const bookTitle = gutenberg?.title || title || firstH1 || fallbackTitle;
  return {
    title: bookTitle,
    author: gutenberg?.author || author,
    language,
    chapters: textChapters(paragraphs, bookTitle),
  };
}
