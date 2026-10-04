// One round of syncing: exchange the books, then fetch the document, merge
// this device's records in, write the result back here and to the server.
// Runs when the app opens, when a book is closed, and on Sync now.
import { encodeDocument, mergeDocuments, parseDocument, sameDocument } from "../Domain/Sync/SyncDocument.js";
import { exchangeBooks } from "./LibrarySync.js";
import { syncConfigured, syncFileUrl } from "./Settings.js";
import { applyDocument, exportAll } from "./SyncStore.js";
import { download, upload } from "./WebDav.js";

/** The server's document; an empty one when there is none yet. */
export async function fetchDocument(settings) {
  const contents = await download(syncFileUrl(settings), settings);
  if (!contents) return { books: [], lookups: [] };
  const document = parseDocument(new TextDecoder().decode(contents));
  if (!document) throw new Error("The sync file on the server could not be read.");
  return document;
}

export function storeDocument(settings, document) {
  return upload(syncFileUrl(settings), encodeDocument(document), settings);
}

/** Merges `remote` in and writes what changed here: `{ merged, applied, uploaded }`, the caller storing `merged` when `uploaded`. */
export async function reconcile(env, remote) {
  const merged = mergeDocuments(await exportAll(env), remote);
  const applied = await applyDocument(env, merged);
  return { merged, applied, uploaded: !sameDocument(merged, remote) };
}

const plural = (count, one, many) => `${count} ${count === 1 ? one : many}`;

/** What a sync did, as a line of text. */
export function syncSummary({ received, sent, applied, uploaded }) {
  const parts = [];
  if (received > 0) parts.push(plural(received, "new book", "new books"));
  if (applied.lookups > 0) parts.push(plural(applied.lookups, "word", "words"));
  if (applied.books > 0) parts.push(plural(applied.books, "book update", "book updates"));
  const came = parts.length ? `Received ${parts.join(", ")}` : "Nothing new here";
  if (!uploaded && sent === 0) return `${came}; the server was up to date.`;
  return `${came}${sent > 0 ? `; sent ${plural(sent, "book", "books")}` : ""}${uploaded ? "; sent changes." : "."}`;
}

let running = null;

/**
 * Brings this device in line with the others: `{ message, failed }`, or
 * null when no server is set up. A sync asked for while one runs joins it.
 */
export function runSync(env) {
  const settings = env.settings.sync();
  if (!syncConfigured(settings)) return Promise.resolve(null);
  running ??= (async () => {
    try {
      // Books first, so the places and groups of any that arrive are applied in this same round.
      const books = await exchangeBooks(env, settings);
      if (books.error) return { message: books.error, failed: true };
      const { merged, applied, uploaded } = await reconcile(env, await fetchDocument(settings));
      if (uploaded) await storeDocument(settings, merged);
      return { message: syncSummary({ received: books.received, sent: books.sent, applied, uploaded }), failed: false };
    } catch (error) {
      return { message: error.message, failed: true };
    } finally {
      running = null;
    }
  })();
  return running;
}
