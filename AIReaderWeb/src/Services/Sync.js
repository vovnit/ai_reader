// One round of syncing: exchange the books (`LibrarySync.js`), then the
// records — find the files in `aireader-sync` that are due, read them, merge
// this device's records in and write the result here, send the files the
// merge changed, and remember where each file stands (`SyncParts.js`).
// Runs when the app opens, when a book is closed, and on Sync now.
import { encodeDocument, parseDocument, sameDocument } from "../Domain/Sync/SyncDocument.js";
import { dueParts, fingerprints, joinParts, mergeParts, oldSyncFile, rememberParts } from "../Domain/Sync/SyncParts.js";
import { exchangeBooks } from "./LibrarySync.js";
import { syncConfigured, syncOldFileUrl, syncPartsUrl } from "./Settings.js";
import { applyDocument, exportAll } from "./SyncStore.js";
import { download, list, upload } from "./WebDav.js";

const empty = () => ({ books: [], lookups: [] });
const text = (contents) => new TextDecoder().decode(contents);

/**
 * What the server has of the due files: `{ remote, listed, old, oldVersion }`
 * — one `{ name, records }` per due file, every file listed with its
 * version, and the old single file's records and version when it has changed
 * since this device last read it.
 */
async function fetchParts(settings, local, known) {
  const folder = syncPartsUrl(settings);
  const listed = (await list(folder, settings)).map(({ name, version }) => ({ name, version, digest: "" }));

  let old = empty();
  let oldVersion = "";
  const oldKnown = known.find((file) => file.name === oldSyncFile)?.version;
  for (const file of await list(syncOldFileUrl(settings), settings)) {
    if (file.name !== oldSyncFile || (file.version && file.version === oldKnown)) continue;
    const contents = await download(syncOldFileUrl(settings), settings);
    // One that cannot be read is passed over rather than stopping every sync
    // from now on.
    old = (contents && parseDocument(text(contents))) || empty();
    oldVersion = file.version;
  }

  const onServer = new Set(listed.map((file) => file.name));
  const remote = [];
  for (const name of dueParts(listed, known, local, old)) {
    const contents = onServer.has(name) ? await download(folder + name, settings) : null;
    const records = contents ? parseDocument(text(contents)) : empty();
    if (!records) throw new Error(`The sync file ${name} on the server could not be read.`);
    remote.push({ name, records });
  }
  return { remote, listed, old, oldVersion };
}

/**
 * Merges the due files with this device's records and writes what changed
 * here: `{ outgoing, settled, applied }` — the files to send, and the due
 * files as this device knows them once those are sent.
 */
async function reconcile(env, fetched) {
  const merged = mergeParts(fetched.remote, await exportAll(env), fetched.old);
  const applied = await applyDocument(env, joinParts(merged));
  const versions = new Map(fetched.listed.map((file) => [file.name, file.version]));
  const digests = new Map(fingerprints(await exportAll(env)).map((file) => [file.name, file.digest]));
  return {
    outgoing: merged.filter((part, index) => !sameDocument(part.records, fetched.remote[index].records)),
    settled: merged.map(({ name }) => ({ name, version: versions.get(name) ?? "", digest: digests.get(name) ?? "" })),
    applied,
  };
}

/**
 * Writes down what this device now knows of the files: `sent` the versions
 * the server gave the files sent. One that could not be sent is left as it
 * was known, so the next sync takes it up again.
 */
async function recordParts(env, settings, round, oldVersion, sent) {
  const unsent = new Set(round.outgoing.map((part) => part.name).filter((name) => !sent.has(name)));
  const settled = round.settled
    .filter((file) => !unsent.has(file.name))
    .map((file) => (sent.has(file.name) ? { ...file, version: sent.get(file.name) } : file));
  // The old file counts as read once everything it brought is on the server.
  if (oldVersion && !unsent.size) settled.push({ name: oldSyncFile, version: oldVersion, digest: "" });
  const folder = syncPartsUrl(settings);
  await env.syncFiles.remember(folder, rememberParts(await env.syncFiles.known(folder), settled));
}

/** The records' round: `{ applied, uploaded }`. Throws when it cannot finish; what was done by then is kept. */
export async function syncRecords(env, settings) {
  const fetched = await fetchParts(settings, await exportAll(env), await env.syncFiles.known(syncPartsUrl(settings)));
  const round = await reconcile(env, fetched);
  const sent = new Map();
  let failure = null;
  try {
    for (const part of round.outgoing) {
      sent.set(part.name, await upload(syncPartsUrl(settings) + part.name, encodeDocument(part.records), settings));
    }
  } catch (error) {
    failure = error;
  }
  await recordParts(env, settings, round, fetched.oldVersion, sent);
  if (failure) throw failure;
  return { applied: round.applied, uploaded: round.outgoing.length > 0 };
}

const plural = (count, one, many) => `${count} ${count === 1 ? one : many}`;

/** What a sync did, as a line of text. */
export function syncSummary({ sent, applied, uploaded }) {
  const parts = [];
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
      const books = await exchangeBooks(env, settings);
      if (books.error) return { message: books.error, failed: true };
      const { applied, uploaded } = await syncRecords(env, settings);
      return { message: syncSummary({ sent: books.sent, applied, uploaded }), failed: false };
    } catch (error) {
      return { message: error.message, failed: true };
    } finally {
      running = null;
    }
  })();
  return running;
}
