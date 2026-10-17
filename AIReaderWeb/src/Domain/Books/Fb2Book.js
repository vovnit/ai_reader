// Reads a FictionBook 2 file, the format most Russian e-books come in, into
// a book for the EPUB builder, `{ title, author, language, chapters }`:
// the title, first author and language from its <title-info>, a chapter
// from each section of its first <body>, and one last chapter from each
// later body (notes, comments). Books are matched across devices by title
// and author and reading places by chapter, so this is the same, word for
// word, as the Kindle app's `Fb2Book` and the iOS app's `FB2Book`.
import { trim } from "../../Support/Text.js";
import { scan } from "../../Support/XmlScanner.js";

// Each is one paragraph: a <p> in a <title> is a heading, as is a
// <subtitle>; a table row is its cells joined.
const paragraphElements = new Set(["p", "v", "subtitle", "text-author", "date", "tr"]);
// Each ends the paragraph before it, as the start or end of a paragraph does.
const blocks = new Set([
  "section", "title", "epigraph", "poem", "stanza", "cite", "annotation", "table",
  "body", "empty-line", "image", "binary", "stylesheet",
]);
// Never hold anything, so they are not kept open even when written without "/>".
const empty = new Set(["empty-line", "image"]);
const nameParts = ["first-name", "middle-name", "last-name"];

/** Every run of ASCII whitespace made one space, none at the ends; a no-break space stays. */
function collapsed(text) {
  return trim(text.replace(/[ \t\r\n]+/g, " "));
}

export function fb2Book(text, fallbackTitle) {
  // The open elements, and how many of each: an end tag closes everything
  // opened after its element, and one that matches nothing is ignored.
  const stack = [];
  const counts = new Map();
  const inside = (name) => (counts.get(name) ?? 0) > 0;

  // The <title-info> fields, each its first element's text.
  const fields = new Map();
  let field = null;
  let authors = 0;

  let bodies = 0;
  const chapters = [];
  const notes = [];
  // The paragraph being read, as cells: one, or a table row's.
  let paragraph = null;
  // The lines of the title naming a chapter: a section's in the first body,
  // the body's own in a later one.
  let titleLines = null;

  const reading = () => inside("body") && !inside("binary") && !inside("stylesheet");
  const chapter = () => (bodies > 1 ? notes : chapters).at(-1);

  function flush() {
    if (!paragraph) return;
    const line = paragraph.cells.map(collapsed).filter((cell) => cell).join(" | ");
    const heading = paragraph.heading;
    paragraph = null;
    if (!line) return;
    titleLines?.push(line);
    chapter().paragraphs.push({ text: line, heading });
  }

  function begin(heading, row) {
    flush();
    paragraph = { cells: [""], heading, row };
  }

  function start(name) {
    if (inside("title-info") && !inside("body")) {
      if (name === "author") authors++;
      const namePart = name === "nickname" || nameParts.includes(name);
      const wanted = namePart ? inside("author") && authors === 1 : name === "book-title" || name === "lang";
      if (wanted && !fields.has(name)) {
        fields.set(name, "");
        field = name;
      }
    }
    if (blocks.has(name)) flush();
    if (name === "body") {
      bodies++;
      if (bodies === 1) chapters.push({ title: "", paragraphs: [] });
      else notes.push({ title: "Notes", paragraphs: [] });
    } else if (reading()) {
      if (name === "section") {
        // A section opens a chapter, unless the one before has nothing but
        // headings yet: then "Part One" and "Chapter 1" open one together.
        if (bodies === 1 && chapter().paragraphs.some((line) => !line.heading)) chapters.push({ title: "", paragraphs: [] });
      } else if (name === "title") {
        if (!titleLines && stack.at(-1) === (bodies === 1 ? "section" : "body")) titleLines = [];
      } else if (paragraphElements.has(name)) {
        begin(inside("title") || name === "subtitle", name === "tr");
      } else if (name === "td" || name === "th") {
        if (!paragraph?.row) begin(inside("title"), true);
        paragraph.cells.push("");
      }
    }
    if (empty.has(name)) return;
    stack.push(name);
    counts.set(name, (counts.get(name) ?? 0) + 1);
  }

  function close(name) {
    if (name === field) field = null;
    if (blocks.has(name) || paragraphElements.has(name)) flush();
    if (name === "title" && titleLines) {
      if (titleLines.length) chapter().title = titleLines.join(" ");
      titleLines = null;
    }
  }

  function end(name) {
    if (!inside(name)) return;
    for (;;) {
      const closed = stack.pop();
      counts.set(closed, counts.get(closed) - 1);
      close(closed);
      if (closed === name) return;
    }
  }

  function addText(run) {
    if (reading()) {
      // Text outside any paragraph still reads, as one of its own.
      if (!paragraph) {
        if (!collapsed(run)) return;
        paragraph = { cells: [""], heading: inside("title"), row: false };
      }
      paragraph.cells[paragraph.cells.length - 1] += run;
    } else if (field) {
      fields.set(field, fields.get(field) + run);
    }
  }

  scan(text, { onStart: start, onEnd: end, onText: addText });
  // A file cut short reads as if everything open closed there.
  while (stack.length) end(stack.at(-1));

  const part = (name) => collapsed(fields.get(name) ?? "");
  const title = part("book-title") || fallbackTitle;
  const author = nameParts.map(part).filter((name) => name).join(" ") || part("nickname");
  const language = part("lang").toLowerCase();
  const kept = [...chapters, ...notes]
    .filter((draft) => draft.paragraphs.length)
    .map((draft) => ({ title: draft.title || title, paragraphs: draft.paragraphs }));
  return { title, author, language, chapters: kept };
}
