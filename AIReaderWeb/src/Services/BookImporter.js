// Takes a book file in as the library keeps every book, an EPUB: an EPUB as
// it is, and a PDF, an FB2 (bare or zipped), a plain-text file or an HTML
// page read into chapters and written as one. The file's name says which it
// is. The same as the Kindle app's `BookImporter`, and the iOS app takes
// files in by the same rules, so a file makes the same book, with the same
// chapters, on every device.
import { fb2Book } from "../Domain/Books/Fb2Book.js";
import { htmlBook } from "../Domain/Books/HtmlBook.js";
import { textBook } from "../Domain/Books/TextBook.js";
import { declaredCharset, decodeBook } from "../Support/Charset.js";
import { ZipArchive } from "../Support/ZipArchive.js";
import { bookIdentifier, proseLanguage, writeEpub } from "./EpubWriter.js";
import { pdfToEpub } from "./PdfImporter.js";

// Name endings, lowercased, and the format each names. ".fb2.zip" is one
// ending, so it leaves the rest of the name as the title, and any ZIP is
// looked into for an FB2, the way FB2 books are shared.
const endings = [
  [".epub", "epub"], [".pdf", "pdf"], [".fb2", "fb2"], [".fb2.zip", "fb2.zip"], [".fbz", "fb2.zip"], [".zip", "fb2.zip"],
  [".txt", "txt"], [".html", "html"], [".htm", "html"], [".xhtml", "html"],
];

/** What the file picker offers: the endings, and the types a phone's picker goes by. */
export const bookFileTypes = ".epub,application/epub+zip,.pdf,application/pdf,.fb2,.fbz,.zip,.txt,text/plain,.html,.htm,.xhtml,text/html";

/** The format a file's name says it is in, and the name without its ending; "" for a name the library does not know. */
export function bookFormat(name) {
  const lower = name.toLowerCase();
  const [ending, format] = endings.find(([ending]) => lower.endsWith(ending)) ?? ["", ""];
  return { format, title: name.slice(0, name.length - ending.length) };
}

export const isBookFile = (name) => bookFormat(name).format !== "";

/** A file's text in the encoding it names, if it names one. */
const declaredText = (bytes) => decodeBook(bytes, declaredCharset(bytes));

/** The bytes of the first FB2 in a ZIP. */
async function zippedFb2(bytes) {
  const zip = new ZipArchive(bytes);
  const name = zip.names().find((entry) => entry.toLowerCase().endsWith(".fb2"));
  if (!name) throw new Error("The ZIP holds no FB2 book.");
  const fb2 = await zip.bytes(name);
  if (!fb2) throw new Error("The FB2 book in the ZIP cannot be read.");
  return fb2;
}

/** The EPUB of a book read from a file of the given kind; its own bytes name it. */
function draftEpub(draft, kind, bytes) {
  if (!draft.chapters.length) throw new Error("The file has no text to read.");
  return writeEpub({
    title: draft.title,
    author: draft.author,
    language: draft.language || proseLanguage(draft.chapters) || "und",
    identifier: bookIdentifier(kind, bytes),
  }, draft.chapters);
}

/**
 * The EPUB the library keeps for a picked, dropped or opened file, as a
 * Blob. A file of a name it does not know is taken for an EPUB, which the
 * library then reads or refuses. Throws why the book cannot be made.
 */
export async function bookEpub(file) {
  const { format, title } = bookFormat(file.name);
  const isPdf = format === "pdf" || file.type === "application/pdf";
  if (!isPdf && (format === "epub" || format === "")) return file;
  const bytes = new Uint8Array(await file.arrayBuffer());
  if (isPdf) return pdfToEpub(bytes, title);
  if (format === "fb2") return draftEpub(fb2Book(declaredText(bytes), title), "fb2", bytes);
  if (format === "fb2.zip") return draftEpub(fb2Book(declaredText(await zippedFb2(bytes)), title), "fb2", bytes);
  if (format === "txt") return draftEpub(textBook(decodeBook(bytes), title), "txt", bytes);
  return draftEpub(htmlBook(declaredText(bytes), title), "html", bytes);
}
