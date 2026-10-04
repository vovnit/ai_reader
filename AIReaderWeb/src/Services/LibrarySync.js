// The books themselves, shared as EPUB files in the sync folder's `Books`.
// A file there this device has not met is fetched and shelved; a book here
// with no file there yet is sent. The browser extension saves web pages
// into the same folder. Removing a book removes it from this device only:
// the file stays for the others, and is not fetched again since it was met.
// Each step is written down as it happens, so a sync that stops halfway
// fetches and sends nothing twice.
import { bookKey } from "../Domain/Books/BookKey.js";
import { isRemoteBook, remoteBookName } from "../Domain/Books/RemoteBookName.js";
import { epubCover, epubMetadata } from "./EpubLoader.js";
import { syncBooksUrl } from "./Settings.js";
import { download, escapeName, list, upload } from "./WebDav.js";

/** `{ received, sent, error }`: how many books came and went, and why the exchange stopped, if it did. */
export async function exchangeBooks(env, settings) {
  const outcome = { received: 0, sent: 0, error: "" };
  const folder = syncBooksUrl(settings);
  const books = await env.library.all();
  const met = await env.library.remoteNamesMet();
  const keys = new Map(books.map((book) => [bookKey(book.title, book.author), book.id]));
  const named = new Set(books.filter((book) => book.remoteName).map((book) => book.id));
  try {
    const remote = (await list(folder, settings)).filter(isRemoteBook);
    // Fetched before anything is sent, so a book this device already has
    // is recognised and not sent a second time.
    for (const name of remote) {
      if (met.has(name)) continue;
      const contents = await download(folder + escapeName(name), settings);
      // A file that is not a readable EPUB is met all the same, so it is
      // not fetched again on every sync.
      await env.library.meetRemote(name);
      if (!contents) continue;
      const file = new Blob([contents], { type: "application/epub+zip" });
      let metadata;
      try {
        metadata = await epubMetadata(file, name);
      } catch {
        continue;
      }
      const key = bookKey(metadata.title, metadata.author);
      const same = keys.get(key);
      if (same === undefined) {
        const cover = await epubCover(file).catch(() => null);
        const id = await env.library.add({ ...metadata, remoteName: name }, file, cover);
        keys.set(key, id);
        named.add(id);
        outcome.received++;
      } else if (!named.has(same)) {
        named.add(same);
        await env.library.setRemoteName(same, name);
      }
    }

    const taken = [...remote];
    for (const book of books) {
      if (named.has(book.id)) continue;
      const file = await env.library.file(book.id);
      if (!file) continue;
      const name = remoteBookName(book.title, book.author, taken);
      await upload(folder + escapeName(name), file, settings, "application/epub+zip");
      taken.push(name);
      await env.library.setRemoteName(book.id, name);
      await env.library.meetRemote(name);
      outcome.sent++;
    }
  } catch (error) {
    outcome.error = error.message;
  }
  return outcome;
}
