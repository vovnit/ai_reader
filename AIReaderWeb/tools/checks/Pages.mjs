// A PDF book's page view below its view: the page's text run together, a
// tapped place in a text item found in it, and the PDF kept beside the book
// that was made from it — with a stand-in for PDF.js, which needs a browser.
import { itemLines, pagePassage } from "../../src/Domain/Books/PagePassage.js";
import { PagesFeature } from "../../src/Features/Pages/PagesFeature.js";
import { check } from "./Check.mjs";
import { freshEnv } from "./Env.mjs";

const item = (str, hasEOL = false) => ({ str, hasEOL });

/** A PDF.js document of pages, each a list of text items. */
const standIn = (pages) => async () => ({
  numPages: pages.length,
  getPage: async (number) => ({ number, getTextContent: async () => ({ items: pages[number - 1] }) }),
});

export async function checkPages() {
  const lines = itemLines([item("Il était une fois un mar"), item("-", true), item("chand qui partit.", true), item(""), item("Fin.")]);
  check("pages: items make lines, each item's place kept", lines.lines.join("|") === "Il était une fois un mar-|chand qui partit.|Fin."
    && lines.places[1].line === 0 && lines.places[1].column === 24 && lines.places[2].line === 1 && lines.places[4].line === 2, JSON.stringify(lines));
  const passage = pagePassage(lines.lines);
  check("pages: lines run together, a hyphenated word mended, each line whole at its start",
    passage.text === "Il était une fois un marchand qui partit. Fin." && passage.text.startsWith("chand", passage.lineStarts[1])
    && passage.text.startsWith("Fin.", passage.lineStarts[2]), JSON.stringify(passage));

  const env = await freshEnv();
  const pdf = new Blob(["%PDF-1.4 stand-in"]);
  const id = await env.library.add({ title: "Contes", author: "", language: "fr" }, new Blob(["epub"]), null, pdf);
  const plain = await env.library.add({ title: "Autre", author: "", language: "fr" }, new Blob(["epub"]), null);
  check("pages: the PDF is kept beside the book made from it, and only that one",
    (await env.library.original(id))?.size === pdf.size && !(await env.library.original(plain)));

  const pages = [
    [item("Page one.", true)],
    [item("Il dit : « Le chat dort", true), item("sur le tapis. » Puis il partit.", true)],
    [item("Fin.")],
  ];
  const reader = { book: { id, language: "fr" }, language: "fr", fraction: 0.5, pdfPage: null };
  const feature = new PagesFeature(env, reader, standIn(pages));
  await feature.load();
  check("pages: opens as far through the PDF as the reader is through the book", feature.status === "loaded" && feature.pageCount === 3 && feature.page === 1);
  const found = feature.wordAt(1, 9);
  check("pages: a tapped word is found with its sentence across the line break, filed under the book",
    found?.word.word === "tapis" && found.word.sentence.startsWith("Il dit : « Le chat dort sur le tapis.") && found.word.bookId === id
    && found.marked.from === 7 && found.marked.to === 12, JSON.stringify(found));
  check("pages: a tap off any word finds none", feature.wordAt(0, 6) === null && feature.wordAt(9, 0) === null);
  const words = feature.words();
  const marked = words.map(({ item, from, to }) => pages[1][item].str.slice(from, to));
  check("pages: every word a tap finds is marked, in its text item, punctuation left out",
    marked.join("|") === "Il|dit|Le|chat|dort|sur|le|tapis|Puis|il|partit" && words[5].item === 1 && words[5].from === 0, JSON.stringify(words));
  check("pages: the words are marked only once asked, and the choice kept", !feature.showsWords
    && (feature.toggleWords(), feature.showsWords) && new PagesFeature(env, reader, standIn(pages)).showsWords);
  feature.toggleWords();
  feature.zoomTo(9);
  const most = feature.zoom;
  feature.zoomTo(0.2);
  check("pages: zoomed from the width of the screen to four times it", most === 4 && feature.zoom === 1);
  feature.next();
  await new Promise((resolve) => setTimeout(resolve, 0));
  check("pages: turned, and remembered for the next opening while the book is open", feature.page === 2 && reader.pdfPage === 2);
  feature.next();
  await new Promise((resolve) => setTimeout(resolve, 0));
  check("pages: no page past the last", feature.page === 2);

  const missing = new PagesFeature(env, { ...reader, book: { id: plain } }, standIn(pages));
  await missing.load();
  check("pages: a book without its PDF here says so", missing.status === "failed" && missing.error.includes("not on this device"));
  await env.library.remove(id);
  check("pages: removing the book removes its PDF", !(await env.library.original(id)));
}
