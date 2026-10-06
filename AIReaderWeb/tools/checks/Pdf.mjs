// A PDF made into an EPUB: one put together here, so the check needs
// nothing from outside — the same file the Kindle app's check makes — and
// a real one when it is given.
import { readFile } from "node:fs/promises";
import { deflateSync } from "node:zlib";
import { pdfToEpub } from "../../src/Services/PdfImporter.js";
import { epubMetadata, loadDocument } from "../../src/Services/EpubLoader.js";
import { PdfDocument } from "../../src/Support/PdfDocument.js";
import { pageLines } from "../../src/Support/PdfPageText.js";
import { check } from "./Check.mjs";

const bytesOf = (text) => Uint8Array.from(text, (c) => c.charCodeAt(0) & 0xff);
const zlibbed = (text) => String.fromCharCode(...deflateSync(bytesOf(text)));
const stream = (dictionary, data) => `<< ${dictionary} /Length ${data.length} >>\nstream\n${data}\nendstream`;

/** A PDF of its objects' bodies, with no cross-reference table: the reader finds objects by scanning. */
function pdfFile(objects, trailer) {
  let out = "%PDF-1.5\n%\xe2\xe3\xcf\xd3\n";
  for (const [number, body] of [...objects].sort((a, b) => a[0] - b[0])) out += `${number} 0 obj\n${body}\nendobj\n`;
  return bytesOf(`${out}trailer\n${trailer}\n%%EOF\n`);
}

/**
 * Four pages with what a printed book's PDF has: running heads, page
 * numbers, a heading, first-line indents, a word hyphenated at a line's
 * end, a paragraph cut by a page break, and two bookmarks.
 */
function samplePdf() {
  const objects = new Map();
  objects.set(1, "<< /Type /Catalog /Pages 2 0 R /Outlines 20 0 R /Lang (fr-FR) /Names << /Dests << /Names [(suite) [4 0 R /Fit]] >> >> >>");
  // Resources and the media box are inherited from the page tree.
  objects.set(2, "<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R 6 0 R] /Count 4 /MediaBox [0 0 612 792] "
    + "/Resources << /Font << /F1 10 0 R /F2 11 0 R >> /XObject << /X1 15 0 R >> >> >>");
  objects.set(3, "<< /Type /Page /Parent 2 0 R /Contents 7 0 R >>");
  objects.set(4, "<< /Type /Page /Parent 2 0 R /Contents 8 0 R >>");
  objects.set(5, "<< /Type /Page /Parent 2 0 R /Contents [9 0 R] >>");
  objects.set(6, "<< /Type /Page /Parent 2 0 R /Contents 16 0 R >>");
  const head = "BT /F1 9 Tf 1 0 0 1 72 770 Tm (Mon Livre) Tj ET\n";
  const folio = (number) => `BT /F1 9 Tf 1 0 0 1 300 40 Tm (${number}) Tj ET\n`;
  objects.set(7, stream("/Filter /FlateDecode", zlibbed(head + String.raw`BT /F1 20 Tf 1 0 0 1 72 740 Tm (Chapitre premier) Tj ET
BT /F1 12 Tf 1 0 0 1 90 700 Tm (Il \351tait une fois un chat qui ai-) Tj
1 0 0 1 72 686 Tm (mait les livres et la musique.) Tj
1 0 0 1 90 672 Tm (Le chat lisait chaque soir, \340 la lumi\350re d\222une) Tj
-18 -14 Td (bougie, des histoires de) Tj ET
` + folio("1"))));
  // A word split by a little kerning stays whole; a wide gap is a space.
  objects.set(8, stream("", head + String.raw`BT /F1 12 Tf 1 0 0 1 72 740 Tm (pirates et de magiciens. Ses amis le trouvaient raf\001n\351.) Tj
1 0 0 1 90 726 Tm [(Un) -280 (jour,) -280 (le) -280 (ch) -20 (at) -280 (partit.)] TJ
/F2 12 Tf 1 0 0 1 90 712 Tm <0001000200030004> Tj ET
` + folio("2")));
  // An inline image whose bytes would show an X if read as content.
  objects.set(9, stream("", `${head}q /X1 Do Q\nBI /W 6 /H 1 /BPC 8 /CS /G ID (X) Tj EI\nBT /F1 12 Tf 1 0 0 1 90 726 Tm (Fin du livre.) Tj ET\n${folio("3")}`));
  objects.set(16, stream("", `${head}BT /F1 12 Tf 1 0 0 1 90 740 Tm (Derni\\350re page.) Tj ET\n${folio("iv")}`));
  objects.set(11, "<< /Type /Font /Subtype /Type0 /BaseFont /Sample /Encoding /Identity-H /DescendantFonts [12 0 R] /ToUnicode 14 0 R >>");
  objects.set(12, "<< /Type /Font /Subtype /CIDFontType2 /BaseFont /Sample /W [1 [600 600 600 500]] >>");
  objects.set(14, stream("", "/CIDInit /ProcSet findresource begin 12 dict begin begincmap\n"
    + "1 begincodespacerange <0000> <FFFF> endcodespacerange\n"
    + "1 beginbfrange <0001> <0003> <0041> endbfrange\n"
    + "1 beginbfchar <0004> <00E9> endbfchar\nendcmap end end"));
  objects.set(15, stream("/Type /XObject /Subtype /Form /BBox [0 0 612 792] /Matrix [1 0 0 1 72 740]", "BT /F1 12 Tf 0 0 Td (Texte dans une forme.) Tj ET"));
  objects.set(20, "<< /Type /Outlines /First 21 0 R /Last 22 0 R /Count 2 >>");
  objects.set(21, "<< /Title (D\\351but) /Parent 20 0 R /Next 22 0 R /Dest [3 0 R /XYZ 0 792 0] >>");
  objects.set(22, "<< /Title <FEFF00530075006900740065> /Parent 20 0 R /Prev 21 0 R /Dest (suite) >>");
  // The simple font and its encoding live in an object stream.
  const font = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding 13 0 R >>";
  const encoding = "<< /Type /Encoding /BaseEncoding /WinAnsiEncoding /Differences [1 /fi] >>";
  const index = `10 0 13 ${font.length + 1} `;
  objects.set(30, stream(`/Type /ObjStm /N 2 /First ${index.length} /Filter /FlateDecode`, zlibbed(`${index}${font} ${encoding}`)));
  objects.set(40, "<< /Title <FEFF004D006F006E0020004C0069007600720065> /Author (Jean Dupont) >>");
  return pdfFile(objects, "<< /Root 1 0 R /Info 40 0 R /Size 41 >>");
}

async function refused(bytes) {
  try {
    await pdfToEpub(bytes, "x");
    return "";
  } catch (error) {
    return error.message;
  }
}

export async function checkPdf(path) {
  const pdf = samplePdf();
  const document = await PdfDocument.open(pdf);
  check("pdf pages come from the page tree", document.pages.length === 4);
  check("pdf information reads UTF-16 and PDFDocEncoding", document.info("Title") === "Mon Livre" && document.info("Author") === "Jean Dupont");
  const outline = document.outline();
  check("pdf bookmarks, direct and named, find their pages", JSON.stringify(outline) === '[{"title":"Début","page":0},{"title":"Suite","page":1}]', JSON.stringify(outline));
  const second = await pageLines(document, document.pages[1], new Map());
  check("pdf text: encodings, a ligature, kerning and a two-byte font", second.length === 5
    && second[1].text === "pirates et de magiciens. Ses amis le trouvaient raffiné." && second[2].text === "Un jour, le chat partit." && second[3].text === "ABCé",
    second.map((line) => line.text).join(" | "));
  check("pdf lines know where they are", Math.abs(second[2].left - 90) < 0.01 && Math.abs(second[2].y - 66) < 0.01 && Math.abs(second[2].size - 12) < 0.01);

  const epub = await pdfToEpub(pdf, "fallback");
  const metadata = await epubMetadata(epub);
  check("pdf epub names the book and its language", metadata.title === "Mon Livre" && metadata.author === "Jean Dupont" && metadata.language === "fr", JSON.stringify(metadata));
  const book = await loadDocument(epub, { withImages: false });
  check("pdf epub has a chapter per bookmark", book.chapters.length === 2 && book.contents.map((entry) => entry.title).join() === "Début,Suite");
  check("pdf paragraphs: heads and folios dropped, a hyphenated word mended, a page break undone",
    book.chapters[0]?.text === "Chapitre premier\nIl était une fois un chat qui aimait les livres et la musique.\n"
      + "Le chat lisait chaque soir, à la lumière d’une bougie, des histoires de pirates et de magiciens. Ses amis le trouvaient raffiné.",
    book.chapters[0]?.text);
  check("pdf text in a form is read, an inline image is not", book.chapters[1]?.text === "Un jour, le chat partit.\nABCé\nTexte dans une forme.\nFin du livre.\nDernière page.", book.chapters[1]?.text);
  check("pdf heading stays a heading", book.chapters[0]?.spans.some((span) => span.kind === "heading"), JSON.stringify(book.chapters[0]?.spans));

  check("not a pdf", (await refused(bytesOf("hello"))).includes("not a PDF"));
  const tiny = (extra, trailer) => pdfFile(new Map([[1, "<< /Type /Catalog /Pages 2 0 R >>"], [2, "<< /Type /Pages /Kids [3 0 R] >>"], ...extra]), trailer);
  check("an encrypted pdf is refused", (await refused(tiny([[3, "<< /Type /Page >>"], [4, "<< /Filter /Standard >>"]], "<< /Root 1 0 R /Encrypt 4 0 R >>"))).includes("encrypted"));
  const scan = await refused(tiny([[3, "<< /Type /Page /Contents 4 0 R >>"], [4, stream("", "q 100 0 0 100 0 0 cm /Im1 Do Q")]], "<< /Root 1 0 R >>"));
  check("a scan without text is sent to ScanTool", scan.includes("ScanTool"), scan);

  if (!path) return;
  const real = await loadDocument(await pdfToEpub(new Uint8Array(await readFile(path)), path.split("/").pop().replace(/\.pdf$/i, "")), { withImages: false });
  const text = real.chapters.reduce((sum, chapter) => sum + chapter.text.length, 0);
  check(`${path.split("/").pop()} becomes an epub`, real.chapters.length > 0 && text > 1000);
  console.log(`      language ${real.language}, ${real.chapters.length} chapters (${real.contents.map((entry) => entry.title).slice(0, 4).join(", ")}…), ${text} characters`);
  console.log(`      ${real.chapters[0].text.slice(0, 300).replaceAll("\n", " ¶ ")}`);
}
