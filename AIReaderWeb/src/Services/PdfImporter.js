// Makes an EPUB of a PDF, so it is read, searched and synced like any other
// book: the PDF's text laid out again as chapters and paragraphs. A scanned
// PDF has no text to take; ScanTool is for those. The same as the Kindle
// app's `PdfImporter`.
import { languageCode } from "../Domain/Books/LanguageDetector.js";
import { pdfChapters } from "../Domain/Books/PdfLayout.js";
import { PdfDocument } from "../Support/PdfDocument.js";
import { pageLines } from "../Support/PdfPageText.js";
import { bookIdentifier, proseLanguage, writeEpub } from "./EpubWriter.js";

const noText = "The PDF has no text to read — it is probably a scan. ScanTool makes an EPUB of a scanned book from its OCR.";

/** The EPUB, as a Blob, for a PDF's bytes; `fallbackTitle` names a PDF that does not name itself. Throws why it cannot be made. */
export async function pdfToEpub(bytes, fallbackTitle) {
  const document = await PdfDocument.open(bytes);
  const fonts = new Map();
  const pages = [];
  for (const page of document.pages) pages.push(await pageLines(document, page, fonts));
  const title = document.info("Title") || fallbackTitle;
  const chapters = pdfChapters(pages, document.outline(), title);
  if (!chapters.length) throw new Error(noText);
  return writeEpub({
    title,
    author: document.info("Author"),
    language: proseLanguage(chapters) || languageCode(document.language()) || "und",
    identifier: bookIdentifier("pdf", bytes),
  }, chapters);
}
