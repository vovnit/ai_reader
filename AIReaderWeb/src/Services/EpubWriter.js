// Writes a book read from another format — a PDF, an FB2, a text or an HTML
// file — as the EPUB the library keeps, so it is read, searched and synced
// like any other. The same as the Kindle app's `EpubWriter`, and the iOS
// app names a book the same way, so the same file makes a book with the
// same identifier on every device.
import { epubFiles } from "../Domain/Books/EpubBuilder.js";
import { detectLanguage } from "../Domain/Books/LanguageDetector.js";
import { ZipWriter } from "../Support/ZipWriter.js";

/** `urn:aireader:<kind>:<FNV-1a 64 of the file's bytes>`, `kind` being pdf, fb2, txt or html. */
export function bookIdentifier(kind, bytes) {
  let hash = 0xcbf29ce484222325n;
  for (const byte of bytes) hash = BigInt.asUintN(64, (hash ^ BigInt(byte)) * 0x100000001b3n);
  return `urn:aireader:${kind}:${hash.toString(16).padStart(16, "0")}`;
}

/** The language the chapters' prose is in, or "" when none stands out. */
export function proseLanguage(chapters) {
  return detectLanguage(chapters.map((chapter) => ({ text: chapter.paragraphs.map((paragraph) => `${paragraph.text}\n`).join("") })));
}

/** The EPUB, as a Blob, of `book`, `{ title, author, language, identifier }`, and its chapters, `[{ title, paragraphs: [{ text, heading }] }]`. */
export async function writeEpub(book, chapters) {
  const modified = new Date().toISOString().replace(/\.\d+Z$/, "Z");
  const zip = new ZipWriter();
  for (const [path, contents] of epubFiles({ ...book, modified }, chapters)) await zip.add(path, contents, { compress: path !== "mimetype" });
  return zip.finish("application/epub+zip");
}
