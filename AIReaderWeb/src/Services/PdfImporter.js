// Makes an EPUB of a PDF, so it is read, searched and synced like any other
// book: the PDF's text laid out again as chapters and paragraphs. A scanned
// PDF has no text to take; ScanTool is for those. The same as the Kindle
// app's `PdfImporter`.
import { epubFiles } from "../Domain/Books/EpubBuilder.js";
import { detectLanguage, languageCode } from "../Domain/Books/LanguageDetector.js";
import { pdfChapters } from "../Domain/Books/PdfLayout.js";
import { PdfDocument } from "../Support/PdfDocument.js";
import { pageLines } from "../Support/PdfPageText.js";
import { ZipWriter } from "../Support/ZipWriter.js";

const noText = "The PDF has no text to read — it is probably a scan. ScanTool makes an EPUB of a scanned book from its OCR.";

/** The same PDF makes a book with the same identifier. */
function identifier(bytes) {
  let hash = 0xcbf29ce484222325n;
  for (const byte of bytes) hash = BigInt.asUintN(64, (hash ^ BigInt(byte)) * 0x100000001b3n);
  return `urn:aireader:pdf:${hash.toString(16).padStart(16, "0")}`;
}

/** The EPUB, as a Blob, for a PDF's bytes; `fallbackTitle` names a PDF that does not name itself. Throws why it cannot be made. */
export async function pdfToEpub(bytes, fallbackTitle) {
  const document = await PdfDocument.open(bytes);
  const fonts = new Map();
  const pages = [];
  for (const page of document.pages) pages.push(await pageLines(document, page, fonts));
  const title = document.info("Title") || fallbackTitle;
  const chapters = pdfChapters(pages, document.outline(), title);
  if (!chapters.length) throw new Error(noText);
  const text = chapters.map((chapter) => ({ text: chapter.paragraphs.map((paragraph) => `${paragraph.text}\n`).join("") }));
  const book = {
    title,
    author: document.info("Author"),
    language: detectLanguage(text) || languageCode(document.language()) || "und",
    identifier: identifier(bytes),
    modified: new Date().toISOString().replace(/\.\d+Z$/, "Z"),
  };
  const zip = new ZipWriter();
  for (const [path, contents] of epubFiles(book, chapters)) await zip.add(path, contents, { compress: path !== "mimetype" });
  return zip.finish("application/epub+zip");
}
