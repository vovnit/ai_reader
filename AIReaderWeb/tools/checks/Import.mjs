// Books in other formats taken in as the library takes them: an FB2 in
// windows-1251, a zipped FB2, a text file and an HTML page put together
// here, each made into an EPUB and read back by the EPUB loader, and a real
// one when it is given.
import { readFile } from "node:fs/promises";
import { LibraryFeature } from "../../src/Features/Library/LibraryFeature.js";
import { bookEpub, bookFormat } from "../../src/Services/BookImporter.js";
import { epubMetadata, loadDocument } from "../../src/Services/EpubLoader.js";
import { bookIdentifier, writeEpub } from "../../src/Services/EpubWriter.js";
import { ZipArchive } from "../../src/Support/ZipArchive.js";
import { ZipWriter } from "../../src/Support/ZipWriter.js";
import { check } from "./Check.mjs";
import { freshEnv } from "./Env.mjs";

const utf8 = (text) => new TextEncoder().encode(text);

/** Text as windows-1251 bytes: ASCII, Russian letters and the few signs used here. */
function cp1251(text) {
  const signs = { 0xa0: 0xa0, 0xab: 0xab, 0xbb: 0xbb, 0x2014: 0x97, 0x401: 0xa8, 0x451: 0xb8 };
  return Uint8Array.from(text, (c) => {
    const code = c.codePointAt(0);
    if (code < 0x80) return code;
    if (code >= 0x410 && code <= 0x44f) return code - 0x410 + 0xc0;
    if (code in signs) return signs[code];
    throw new Error(`no windows-1251 byte for ${c}`);
  });
}

const russian = "Он не знал, что ему делать, и все было так, как она сказала. Я думал, что это было не у него, а у меня. ".repeat(4);
const french = "Le petit prince arriva sur la planète. Il regarda les étoiles et dit que la nuit était belle. ".repeat(10);

const fb2 = `<?xml version="1.0" encoding="windows-1251"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info>
<author><first-name>Лев</first-name><last-name>Толстой</last-name></author><book-title>Война и мир</book-title><lang>ru</lang>
</title-info></description><body>
<section><title><p>Глава первая</p></title><p>— Ну что, князь, «Генуя» и Лукка?</p></section>
<section><title><p>Глава вторая</p></title><p>Анна Павловна кашляла.</p></section>
</body></FictionBook>`;

// No title of its own, so the file's name gives it one, and no language.
const zippedFb2 = `<?xml version="1.0" encoding="utf-8"?>
<FictionBook><description><title-info><book-title></book-title></title-info></description><body><p>${french}</p></body></FictionBook>`;

const text = `Глава 1\r\n\r\n${russian}\r\n\r\nГлава 2\r\n\r\n${russian}\r\n`;

const html = `<!DOCTYPE html><html lang="FR"><head><meta charset="utf-8"><title>Contes</title><meta name="author" content="Charles Perrault"></head>
<body><h2>Le Petit Chaperon rouge</h2><p>Il était une fois une petite fille.</p><h2>La Barbe bleue</h2><p>Il était une fois un homme.</p></body></html>`;

async function zipOf(entries, compress = true) {
  const zip = new ZipWriter();
  for (const [name, contents] of entries) await zip.add(name, contents, { compress });
  return new Uint8Array(await (await zip.finish("application/zip")).arrayBuffer());
}

/** What the importer makes of the bytes under that name and type: the EPUB's metadata, identifier, contents and chapter texts, or why it refused. */
async function imported(bytes, name, type = "") {
  try {
    const epub = await bookEpub(new File([bytes], name, { type }));
    const opf = await (await ZipArchive.open(epub)).text("OEBPS/content.opf");
    const book = await loadDocument(epub, { withImages: false });
    return {
      ...(await epubMetadata(epub)),
      identifier: /<dc:identifier[^>]*>([^<]*)</.exec(opf)?.[1],
      contents: book.contents.map((entry) => entry.title).join(" | "),
      chapters: book.chapters.map((chapter) => chapter.text),
    };
  } catch (error) {
    return { refused: error.message };
  }
}

export async function checkImport(path) {
  const fb2Bytes = cp1251(fb2);
  const war = await imported(fb2Bytes, "voina.fb2");
  check("import: an fb2 in windows-1251, its title, author and language", war.title === "Война и мир" && war.author === "Лев Толстой" && war.language === "ru"
    && war.identifier === bookIdentifier("fb2", fb2Bytes), JSON.stringify(war));
  check("import: the fb2's sections are the epub's chapters and contents", war.contents === "Глава первая | Глава вторая"
    && war.chapters.join(" || ") === "Глава первая\n— Ну что, князь, «Генуя» и Лукка? || Глава вторая\nАнна Павловна кашляла.", JSON.stringify(war));

  const zipBytes = await zipOf([["LISEZMOI.txt", "Lisez-moi."], ["livre/petit.fb2", zippedFb2], ["autre.fb2", "<FictionBook/>"]]);
  const zipped = await imported(zipBytes, "Le Petit Livre.FB2.ZIP");
  check("import: a zipped fb2, its first fb2 read, named by the zip's name without .fb2.zip, its language from its prose",
    zipped.title === "Le Petit Livre" && zipped.author === "" && zipped.language === "fr" && zipped.identifier === bookIdentifier("fb2", zipBytes)
      && zipped.contents === "Le Petit Livre" && zipped.chapters[0] === french.trim(), JSON.stringify(zipped));

  // The resource forks a Mac adds, which end .fb2 too, come first here, and a
  // second entry of the book's name after it.
  const fork = "\0\x05\x16\x07";
  const macBytes = await zipOf([["__MACOSX/._petit.fb2", fork], ["._petit.fb2", fork], ["livre/petit.fb2", zippedFb2],
    ["livre/petit.fb2", "<FictionBook><body><p>Pas celui-ci.</p></body></FictionBook>"]]);
  const mac = await imported(macBytes, "Mac.zip");
  check("import: a zip's first fb2 is neither a Mac's resource fork nor a later entry of the same name",
    mac.title === "Mac" && mac.chapters[0] === french.trim(), JSON.stringify(mac));

  const textBytes = cp1251(text);
  const onegin = await imported(textBytes, "Онегин.txt");
  check("import: a text file in windows-1251 that does not say so, named by its file", onegin.title === "Онегин" && onegin.language === "ru"
    && onegin.identifier === bookIdentifier("txt", textBytes) && onegin.contents === "Глава 1 | Глава 2"
    && onegin.chapters[1] === `Глава 2\n${russian.trim()}`, JSON.stringify(onegin));

  const htmlBytes = utf8(html);
  const tales = await imported(htmlBytes, "contes.htm");
  check("import: an html page, its title, author, language and chapters", tales.title === "Contes" && tales.author === "Charles Perrault" && tales.language === "fr"
    && tales.identifier === bookIdentifier("html", htmlBytes) && tales.contents === "Le Petit Chaperon rouge | La Barbe bleue"
    && tales.chapters[0] === "Le Petit Chaperon rouge\nIl était une fois une petite fille.", JSON.stringify(tales));
  const short = await imported(utf8("<p>Short.</p>"), "short.xhtml");
  check("import: a language neither named nor told by the prose is und", short.language === "und" && short.title === "short", JSON.stringify(short));
  const bare = [await imported(utf8("Il était une fois."), ".TXT"), await imported(utf8(zippedFb2), ".fb2")];
  check("import: a name that is all ending titles a book that names none Book",
    bare.every((book) => book.title === "Book" && book.contents === "Book"), JSON.stringify(bare));
  const typedText = await imported(utf8("Il était une fois."), "conte", "text/plain");
  const typedHtml = await imported(htmlBytes, "contes.php", "text/html");
  check("import: a name not known is read by its type, text or HTML, as the picker offers them, the name whole its title",
    typedText.title === "conte" && typedText.chapters[0] === "Il était une fois." && typedHtml.title === "Contes"
      && (await imported(utf8("Il était une fois."), "conte")).refused, JSON.stringify([typedText, typedHtml]));

  check("import: a file with no text is refused", (await imported(utf8("\r\n \n"), "empty.txt")).refused === "The file has no text to read.");
  const noFb2 = await zipOf([["book.epub", "not an fb2"]]);
  check("import: a zip without an fb2 is refused", (await imported(noFb2, "book.zip")).refused === "The ZIP holds no FB2 book.");
  // A method this reader does not inflate (12 is bzip2), in the local header and the central directory both.
  const bz = await zipOf([["book.fb2", "<FictionBook><body><p>Text.</p></body></FictionBook>"]], false);
  bz[8] = 12;
  bz[bz.findIndex((byte, i) => byte === 0x50 && bz[i + 1] === 0x4b && bz[i + 2] === 1 && bz[i + 3] === 2) + 10] = 12;
  check("import: an fb2 in a zip that cannot be unpacked is refused", (await imported(bz, "bz.fb2.zip")).refused === "The FB2 book in the ZIP cannot be read.");
  const petit = await writeEpub({ title: "Le Petit Prince", author: "", language: "fr", identifier: "urn:uuid:petit" },
    [{ title: "I", paragraphs: [{ text: "Lorsque j'avais six ans.", heading: false }] }]);
  const renamed = new File([petit], "Petit.ZIP");
  check("import: a zip holding an epub, not an fb2, is that epub, as it is", (await bookEpub(renamed)) === renamed);
  const epub = new File([utf8("anything")], "book.EPUB");
  check("import: an epub, or a file of a name not known, is kept as it is", (await bookEpub(epub)) === epub);
  check("import: formats are told by the name's ending, in any case", JSON.stringify(["a.Fb2", "b.fbz", "c.HTM", "d.TXT", "e.docx"].map((name) => bookFormat(name)))
    === JSON.stringify([{ format: "fb2", title: "a" }, { format: "fb2.zip", title: "b" }, { format: "html", title: "c" }, { format: "txt", title: "d" }, { format: "", title: "e.docx" }]));
  check("import: the identifier is FNV-1a 64 of the file, as before", bookIdentifier("pdf", utf8("hello")) === "urn:aireader:pdf:a430d84680aabd0b");

  const library = new LibraryFeature(await freshEnv());
  const added = await library.add([new File([fb2Bytes], "voina.fb2"), new File([noFb2], "other.zip"), new File([fb2Bytes], "again.fb2"), renamed]);
  check("import: the library puts an fb2 and an epub renamed .zip on the shelf and says why others were not",
    added.length === 2 && library.books.map((book) => book.title).sort().join(" | ") === "Le Petit Prince | Война и мир"
      && library.message === "other.zip: The ZIP holds no FB2 book.\n“Война и мир” is already on the shelf.", library.message);

  if (!path) return;
  const name = path.split("/").pop();
  const real = await imported(await readFile(path), name);
  check(`${name} becomes an epub`, !real.refused && real.chapters.length > 0 && real.chapters.join("").length > 1000, real.refused);
  if (real.refused) return;
  console.log(`      “${real.title}” by ${real.author || "nobody named"}, language ${real.language}, ${real.chapters.length} chapters (${real.contents.split(" | ").slice(0, 4).join(", ")}…), ${real.chapters.join("").length} characters`);
  console.log(`      ${real.chapters[0].slice(0, 300).replaceAll("\n", " ¶ ")}`);
}
