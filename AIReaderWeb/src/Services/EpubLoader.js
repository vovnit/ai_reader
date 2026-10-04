// Reads an `.epub` kept as a Blob: its metadata for the shelf, its chapters
// for reading, and its cover for the list. Chapters are kept and dropped by
// the Kindle app's rules, so chapter numbers agree between devices.
import { contentsEntries } from "../Domain/Books/Contents.js";
import {
  coverItem, directoryOf, packagePath, parsePackage, readingOrder, resolve, unescapeHref,
} from "../Domain/Books/EpubPackage.js";
import { plainText } from "../Domain/Books/HtmlText.js";
import { detectLanguage, languageCode } from "../Domain/Books/LanguageDetector.js";
import { isBlank } from "../Support/Text.js";
import { ZipArchive } from "../Support/ZipArchive.js";

const imageTypes = { jpg: "image/jpeg", jpeg: "image/jpeg", png: "image/png", gif: "image/gif", svg: "image/svg+xml", webp: "image/webp" };

async function openPackage(blob) {
  let archive;
  try {
    archive = await ZipArchive.open(blob);
  } catch {
    throw new Error("The file is not an EPUB: it is not a ZIP archive.");
  }
  const containerPath = archive.find("META-INF/container.xml");
  const path = containerPath ? packagePath((await archive.text(containerPath)) ?? "") : "";
  if (!path) throw new Error("The EPUB has no META-INF/container.xml.");
  const opf = await archive.text(path);
  if (opf === null) throw new Error("The EPUB has no package document.");
  return { archive, pkg: parsePackage(opf), directory: directoryOf(path) };
}

/** `{ title, author, language }`; the title falls back to the file's name. Throws why the file cannot be read. */
export async function epubMetadata(blob, fileName = "") {
  const { pkg } = await openPackage(blob);
  return { title: pkg.title || fileName.replace(/\.epub$/i, "") || "Untitled", author: pkg.author, language: pkg.language };
}

function mediaType(pkg, directory, entry) {
  for (const item of pkg.items.values()) if (resolve(item.href, directory) === entry && item.mediaType) return item.mediaType;
  return imageTypes[entry.split(".").pop().toLowerCase()] ?? "application/octet-stream";
}

/**
 * The book for reading: `{ chapters, contents, language }`, each chapter as
 * `plainText` makes it. With `withImages` each picture carries a `blob`;
 * without, a search spares reading them, and chapters still number alike.
 */
export async function loadDocument(blob, { withImages = true } = {}) {
  const { archive, pkg, directory } = await openPackage(blob);
  const chapters = [];
  // Which chapter each file became, for the table of contents.
  const chapterByEntry = new Map();
  for (const item of readingOrder(pkg)) {
    const entry = resolve(item.href, directory);
    const markup = await archive.text(entry);
    if (markup === null) continue;
    const chapter = plainText(markup);
    let hasPictures = false;
    for (const image of chapter.images) {
      if (image.source.includes("://") || image.source.startsWith("data:")) continue;
      const imageEntry = resolve(unescapeHref(image.source), directoryOf(entry));
      if (withImages) {
        const bytes = await archive.bytes(imageEntry);
        if (bytes) image.blob = new Blob([bytes], { type: mediaType(pkg, directory, imageEntry) });
        hasPictures ||= !!bytes;
      } else {
        hasPictures ||= archive.has(imageEntry);
      }
    }
    if (isBlank(chapter.text) && !hasPictures) continue;
    chapterByEntry.set(entry, chapters.length);
    chapters.push(chapter);
  }
  if (!chapters.length) throw new Error("The book has no readable content.");
  const contents = await contentsEntries(archive, pkg, directory, chapterByEntry, chapters);
  // EPUB metadata is often wrong — plenty of French books declare
  // themselves English — so the prose has the final say.
  const language = detectLanguage(chapters) || languageCode(pkg.language);
  return { chapters, contents, language };
}

/** The cover image, if the book has one. */
export async function epubCover(blob) {
  const { archive, pkg, directory } = await openPackage(blob);
  const item = coverItem(pkg);
  if (!item) return null;
  const entry = resolve(item.href, directory);
  const bytes = await archive.bytes(entry);
  return bytes ? new Blob([bytes], { type: item.mediaType || mediaType(pkg, directory, entry) }) : null;
}
