// Takes a book file in as the library keeps every book, an EPUB: an EPUB as
// it is, and a PDF, an FB2 (bare or zipped), a plain-text file or an HTML
// page read into chapters and written as one. The file's name says which it
// is, or for a name not known, the type the picker gave it. The same as the
// Kindle app's `BookImporter`, and the iOS app takes files in by the same
// rules, so a file makes the same book, with the same chapters, on every
// device.
import { fb2Book } from "../Domain/Books/Fb2Book.js";
import { htmlBook } from "../Domain/Books/HtmlBook.js";
import { textBook } from "../Domain/Books/TextBook.js";
import { declaredCharset, decodeBook } from "../Support/Charset.js";
import { ZipArchive } from "../Support/ZipArchive.js";
import { bookIdentifier, proseLanguage, writeEpub } from "./EpubWriter.js";
import { pdfToEpub } from "./PdfImporter.js";

// Name endings, lowercased, and the format each names. ".fb2.zip" is one
// ending, so it leaves the rest of the name as the title, and any ZIP is
// looked into for an FB2, the way FB2 books are shared, or else an EPUB.
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

/** Whether the file is a PDF, which the library keeps beside its EPUB for the page view. */
export const isPdfFile = (file) => bookFormat(file.name).format === "pdf" || file.type === "application/pdf";

/** A file's text in the encoding it names, if it names one. */
const declaredText = (bytes) => decodeBook(bytes, declaredCharset(bytes));

/** Whether a ZIP entry is an FB2, and not the resource fork a Mac adds beside a file ("__MACOSX/", "._"), which ends the same. */
function isFb2Entry(entry) {
  const base = entry.slice(entry.lastIndexOf("/") + 1);
  return entry.toLowerCase().endsWith(".fb2") && !entry.startsWith("__MACOSX/") && !base.startsWith("._");
}

/** The bytes of the first FB2 in a ZIP, or null for a ZIP that is an EPUB. */
async function zippedFb2(bytes) {
  const zip = new ZipArchive(bytes);
  const name = zip.names().find(isFb2Entry);
  // An EPUB is a ZIP too, and one renamed .zip is still a book.
  if (!name && zip.find("META-INF/container.xml")) return null;
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

/** The formats a file of a name not known is in, by the type the picker gave it. */
const typeFormats = { "text/plain": "txt", "text/html": "html" };

/**
 * The EPUB the library keeps for a picked, dropped or opened file, as a
 * Blob. A file of a name it does not know is read by its type when that is
 * text or HTML, as the picker offers them, and its whole name is the title;
 * otherwise it is taken for an EPUB, which the library then reads or
 * refuses. Throws why the book cannot be made.
 */
export async function bookEpub(file) {
  const named = bookFormat(file.name);
  const format = named.format || typeFormats[file.type] || "";
  const isPdf = isPdfFile(file);
  if (!isPdf && (format === "epub" || format === "")) return file;
  const bytes = new Uint8Array(await file.arrayBuffer());
  if (isPdf) return pdfToEpub(bytes, named.title);
  // A name that is all ending, ".txt", leaves no title for a book that names none.
  const title = named.title || "Book";
  if (format === "fb2") return draftEpub(fb2Book(declaredText(bytes), title), "fb2", bytes);
  if (format === "fb2.zip") {
    const fb2 = await zippedFb2(bytes);
    return fb2 ? draftEpub(fb2Book(declaredText(fb2), title), "fb2", bytes) : file;
  }
  if (format === "txt") return draftEpub(textBook(decodeBook(bytes), title), "txt", bytes);
  return draftEpub(htmlBook(declaredText(bytes), title), "html", bytes);
}
