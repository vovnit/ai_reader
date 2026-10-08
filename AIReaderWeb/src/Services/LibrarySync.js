// The books themselves, shared as EPUB files in the sync folder's `Books`.
// A sync lists the folder, so the library can show what is there, and
// sends a book here that has no file there yet. A file is fetched only when
// asked for. The browser extension saves web pages into the same folder.
// A book here without a file is matched by the name it would be given, so a
// book added on two devices separately ends up on the server once. Each
// step is written down as it happens, so a sync that stops halfway sends
// nothing twice.
import { bookKey } from "../Domain/Books/BookKey.js";
import { isRemoteBook, remoteBookName } from "../Domain/Books/RemoteBookName.js";
import { epubCover, epubMetadata } from "./EpubLoader.js";
import { syncBooksUrl } from "./Settings.js";
import { download, escapeName, list, remove, upload } from "./WebDav.js";

/** `{ sent, error }`: how many books went, and why the exchange stopped, if it did. */
export async function exchangeBooks(env, settings) {
  const outcome = { sent: 0, error: "" };
  const folder = syncBooksUrl(settings);
  let taken = null;
  try {
    const remote = (await list(folder, settings)).map((file) => file.name).filter(isRemoteBook);
    // Compared without case, since some servers ignore it.
    const byLowercase = new Map(remote.map((name) => [name.toLowerCase(), name]));
    taken = [...remote];
    for (const book of await env.library.all()) {
      if (book.remoteName) continue;
      const same = byLowercase.get(remoteBookName(book.title, book.author, []).toLowerCase());
      if (same) {
        await env.library.setRemoteName(book.id, same);
        continue;
      }
      const file = await env.library.file(book.id);
      if (!file) continue;
      const name = remoteBookName(book.title, book.author, taken);
      await upload(folder + escapeName(name), file, settings, "application/epub+zip");
      taken.push(name);
      await env.library.setRemoteName(book.id, name);
      outcome.sent++;
    }
  } catch (error) {
    outcome.error = error.message;
  }
  // What was listed, and sent by the time it stopped.
  if (taken) await env.library.setRemoteNames(taken);
  return outcome;
}

/** Fetches one file and shelves it; a book already here that has no file yet takes it as its own instead. Throws when it cannot. */
export async function fetchBook(env, settings, name) {
  const contents = await download(syncBooksUrl(settings) + escapeName(name), settings);
  if (!contents) {
    await env.library.forgetRemote(name);
    throw new Error("It is no longer in the sync folder.");
  }
  const file = new Blob([contents], { type: "application/epub+zip" });
  const metadata = await epubMetadata(file, name);
  const key = bookKey(metadata.title, metadata.author);
  const same = (await env.library.all()).find((book) => bookKey(book.title, book.author) === key);
  if (same?.remoteName) throw new Error(`“${same.title}” is already on the shelf.`);
  if (same) return env.library.setRemoteName(same.id, name);
  const cover = await epubCover(file).catch(() => null);
  await env.library.add({ ...metadata, remoteName: name }, file, cover);
}

/** Deletes one file from the folder; devices that have the book keep their copy. */
export async function deleteRemoteBook(env, settings, name) {
  await remove(syncBooksUrl(settings) + escapeName(name), settings);
  await env.library.forgetRemote(name);
}
