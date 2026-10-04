// A whole EPUB through the loader: one made here, so the check needs
// nothing from outside, and a real book when one is given.
import { openAsBlob } from "node:fs";
import { ZipWriter } from "../../../BrowserExtension/lib/zip.js";
import { chapterParagraphs } from "../../src/Domain/Reading/ChapterParagraphs.js";
import { BookCorpus } from "../../src/Services/BookCorpus.js";
import { epubCover, epubMetadata, loadDocument } from "../../src/Services/EpubLoader.js";
import { check } from "./Check.mjs";
import { freshEnv } from "./Env.mjs";

async function madeBook() {
  const zip = new ZipWriter();
  await zip.add("mimetype", "application/epub+zip", { compress: false });
  await zip.add("META-INF/container.xml", '<?xml version="1.0"?><container><rootfiles><rootfile full-path="OEBPS/content.opf"/></rootfiles></container>');
  await zip.add("OEBPS/content.opf", `<package><metadata><dc:title>Le Petit Livre</dc:title><dc:creator>Un Auteur</dc:creator><dc:language>en</dc:language></metadata>
<manifest><item id="c" href="text/one.xhtml" media-type="application/xhtml+xml"/><item id="blank" href="text/blank.xhtml" media-type="application/xhtml+xml"/>
<item id="two" href="text/two.xhtml" media-type="application/xhtml+xml"/><item id="img" href="images/a%20b.png" media-type="image/png" properties="cover-image"/>
<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/></manifest>
<spine><itemref idref="c"/><itemref idref="blank"/><itemref idref="two"/></spine></package>`);
  const prose = "Le petit prince arriva sur la planète. Il regarda les étoiles et dit que la nuit était belle. ".repeat(10);
  await zip.add("OEBPS/text/one.xhtml", `<html><body><h1>Un</h1><p>${prose}</p><p id="deux">Deuxième partie.</p></body></html>`);
  await zip.add("OEBPS/text/blank.xhtml", "<html><body><p> </p></body></html>");
  await zip.add("OEBPS/text/two.xhtml", '<html><body><h1>Deux</h1><p><img src="../images/a%20b.png"/></p><p>Le prince revint.</p></body></html>');
  await zip.add("OEBPS/images/a b.png", new Uint8Array([137, 80, 78, 71]));
  await zip.add("OEBPS/nav.xhtml", '<html><body><nav epub:type="toc"><ol><li><a href="text/one.xhtml">Un</a><ol><li><a href="text/one.xhtml#deux">Partie deux</a></li></ol></li>'
    + '<li><a href="text/blank.xhtml">Blanc</a></li><li><a href="text/two.xhtml">Deux</a></li></ol></nav></body></html>');
  return zip.finish("application/epub+zip");
}

export async function checkEpub(path) {
  const blob = await madeBook();
  const metadata = await epubMetadata(blob, "x.epub");
  check("epub metadata", metadata.title === "Le Petit Livre" && metadata.author === "Un Auteur" && metadata.language === "en");
  const document = await loadDocument(blob);
  check("a blank chapter is dropped, so chapters number alike everywhere", document.chapters.length === 2 && document.chapters[1].text.startsWith("Deux"));
  check("the prose has the final say on the language", document.language === "fr", document.language);
  check("contents point into the kept chapters, anchors to their place", document.contents.length === 3
    && document.contents[1].title === "Partie deux" && document.chapters[0].text.slice(document.contents[1].offset).startsWith("Deuxième")
    && document.contents[2].chapter === 1, JSON.stringify(document.contents));
  check("pictures are read with their type", document.chapters[1].images[0]?.blob?.type === "image/png");
  check("the cover", (await epubCover(blob))?.size === 4);
  const paragraphs = chapterParagraphs(document.chapters[1]);
  check("a chapter's paragraphs: a heading, a picture, prose", paragraphs.length === 3 && paragraphs[0].heading && paragraphs[1].image === 0 && paragraphs[2].runs[0].start === paragraphs[2].start);
  const withoutImages = await loadDocument(blob, { withImages: false });
  check("without pictures, the same chapters", withoutImages.chapters.length === 2 && !withoutImages.chapters[1].images[0].blob);

  const env = await freshEnv();
  const one = await env.library.add(metadata, blob, null);
  const two = await env.library.add({ ...metadata, title: "Tome 2" }, blob, null);
  const corpus = new BookCorpus([await env.library.find(one), await env.library.find(two)], env.library);
  const hits = await corpus.search("prince", 100, { bookId: one, chapter: 0, offset: 200 });
  const mine = hits.filter((hit) => hit.bookId === one);
  check("a corpus search stops where the reader is in the open book, not in the others", mine.length === 2 && mine.every((hit) => hit.chapter === 0 && hit.offset < 200)
    && hits.filter((hit) => hit.bookId === two).length === 11, hits.map((hit) => `${hit.bookTitle}:${hit.chapter}:${hit.offset}`).join());

  if (!path) return;
  const book = await openAsBlob(path);
  const real = await loadDocument(book);
  const text = real.chapters.reduce((sum, chapter) => sum + chapter.text.length, 0);
  check(`${path.split("/").pop()} loads`, real.chapters.length > 0 && text > 1000);
  console.log(`      language ${real.language}, ${real.chapters.length} chapters, ${real.contents.length} contents entries, ${text} characters, cover ${(await epubCover(book)) ? "yes" : "no"}`);
}
