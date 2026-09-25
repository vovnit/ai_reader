// The two ways a page becomes a book: on its own at once, or as one page of
// a book composed over time and saved when it is complete.
import { capturePage } from "./capture.js";
import { buildEpub } from "./epub.js";
import { remoteBookName } from "./names.js";
import { clearDraft, loadDraft, saveDraft } from "./storage.js";
import { listBooks, putBook } from "./webdav.js";

/** Binds the book and stores it in the books folder; returns its file name. */
async function store(book, settings) {
  const blob = await buildEpub(book);
  const name = remoteBookName(book.title, book.author, await listBooks(settings));
  await putBook(name, blob, settings);
  return name;
}

export async function savePage(extracted, settings) {
  const page = await capturePage(extracted, "page");
  return store(
    {
      title: page.title,
      author: page.byline || page.site,
      language: page.language,
      pages: [page],
      images: page.images,
    },
    settings
  );
}

export async function addPage(extracted) {
  const draft = await loadDraft();
  const page = await capturePage(extracted, `p${Date.now().toString(36)}`);
  draft.pages.push(page);
  draft.title ||= page.title;
  await saveDraft(draft);
  return draft;
}

export async function removePage(index) {
  const draft = await loadDraft();
  draft.pages.splice(index, 1);
  if (draft.pages.length === 0) {
    await clearDraft();
    return loadDraft();
  }
  await saveDraft(draft);
  return draft;
}

export async function renameDraft(title) {
  const draft = await loadDraft();
  draft.title = title;
  await saveDraft(draft);
}

/** Saves the book in progress and starts afresh. */
export async function saveComposed(settings) {
  const draft = await loadDraft();
  if (draft.pages.length === 0) throw new Error("The book has no pages yet.");
  // Pages from one site are that site's book; from several, nobody's.
  const sites = new Set(draft.pages.map((page) => page.site));
  const name = await store(
    {
      title: draft.title.trim() || draft.pages[0].title,
      author: sites.size === 1 ? [...sites][0] : "",
      language: draft.pages[0].language,
      pages: draft.pages,
      images: draft.pages.flatMap((page) => page.images),
    },
    settings
  );
  await clearDraft();
  return name;
}
