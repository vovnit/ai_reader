// Against a real WebDAV server, when `AIREADER_SYNC_URL` (and `_USER`,
// `_PASSWORD`) name one: three devices share a book through the folder —
// the second sees it, fetches it on request and removes it again, the third
// has it already — then the file is deleted there; a sync carries a place
// and a word between them; and the records, the Kindle check's way. Node
// sends no Origin, so this checks the protocol, not the server's CORS; the
// browser does that.
import { bookRecord, encodeDocument, lookupRecord } from "../../src/Domain/Sync/SyncDocument.js";
import { splitDocument } from "../../src/Domain/Sync/SyncParts.js";
import { runSync, syncRecords } from "../../src/Services/Sync.js";
import { deleteRemoteBook, exchangeBooks, fetchBook } from "../../src/Services/LibrarySync.js";
import { syncOldFileUrl, syncPartsUrl } from "../../src/Services/Settings.js";
import { upload } from "../../src/Services/WebDav.js";
import { check, skip } from "./Check.mjs";
import { freshEnv } from "./Env.mjs";
import { epubMetadata } from "../../src/Services/EpubLoader.js";

const failure = (promise) => promise.then(() => "", (error) => error.message);

export async function checkServer(book) {
  const base = process.env.AIREADER_SYNC_URL;
  if (!base) return skip("WebDAV server", "set AIREADER_SYNC_URL to check against one");
  if (!book) return skip("WebDAV server", "give a book to share");
  const settings = { url: `${base.replace(/\/+$/, "")}/web-${process.pid}`, username: process.env.AIREADER_SYNC_USER ?? "", password: process.env.AIREADER_SYNC_PASSWORD ?? "" };
  const device = async () => {
    const env = await freshEnv();
    env.settings.saveSync(settings);
    return env;
  };

  const a = await device();
  const id = await a.library.add(await epubMetadata(book), book, null);
  const sent = await exchangeBooks(a, settings);
  const name = (await a.library.find(id)).remoteName;
  check("first device sends its book", !sent.error && sent.sent === 1 && (await a.library.remoteNames()).has(name), sent.error);
  const quiet = await exchangeBooks(a, settings);
  check("first device then has nothing to do", !quiet.error && quiet.sent === 0, quiet.error);

  const b = await device();
  const listed = await exchangeBooks(b, settings);
  check("second device lists it without fetching it", !listed.error && listed.sent === 0 && !(await b.library.all()).length
    && (await b.library.remoteNames()).has(name), listed.error);
  const fetched = await failure(fetchBook(b, settings, name));
  const shelf = await b.library.all();
  check("second device fetches it on request", !fetched && shelf.length === 1 && shelf[0].remoteName === name
    && (await b.library.file(shelf[0].id)).size === book.size, fetched);
  const twice = await failure(fetchBook(b, settings, name));
  check("a book on the shelf is not fetched twice", twice && (await b.library.all()).length === 1, twice);
  await b.library.remove(shelf[0].id);
  const removed = await exchangeBooks(b, settings);
  check("a book removed here stays in the folder", !removed.error && (await b.library.remoteNames()).has(name), removed.error);

  const c = await device();
  await c.library.add(await epubMetadata(book), book, null);
  const matched = await exchangeBooks(c, settings);
  check("a book already here is recognised, not fetched or sent", !matched.error && matched.sent === 0
    && (await c.library.all())[0].remoteName === name, matched.error);

  await a.library.savePosition(id, 1, 10, { chapter: 1, fraction: 0.1, snippet: "Il vint" });
  await a.lookups.save({ word: "maisons", sentence: "Les maisons.", language: "fr", bookId: id }, { lemma: "maison", formNote: "pl.", meaning: "дом", guessed: false, confidence: 0.9 });
  const first = await runSync(a);
  check("sync server round trip", first && !first.failed, first?.message);
  const second = await runSync(a);
  check("second sync has nothing to send", second?.message.endsWith("the server was up to date."), second?.message);
  const other = await runSync(c);
  const place = (await c.library.all())[0];
  check("the other device takes the word and the place", !other.failed && (await c.lookups.all()).length === 1 && place.placePending && place.place.snippet === "Il vint", other.message);
  console.log(`      ${first.message}`);

  await deleteRemoteBook(c, settings, name);
  check("deleting the file from the folder", !(await c.library.remoteNames()).size);
  const after = await exchangeBooks(a, settings);
  check("a deleted file is not sent again", !after.error && after.sent === 0 && !(await a.library.remoteNames()).size, after.error);
  const missing = await failure(fetchBook(b, settings, name));
  check("fetching a deleted file says so", missing === "It is no longer in the sync folder." && !(await b.library.all()).length, missing);
}

/**
 * The records alone, in a folder of their own: a library an older version
 * left in the one file is moved over by the first device to sync, reaches a
 * second device, and from then on a change travels as the one file it falls
 * in. A device not yet updated still gets its changes through the old file.
 */
export async function checkRecordsServer() {
  const base = process.env.AIREADER_SYNC_URL;
  if (!base) return;
  const settings = { url: `${base.replace(/\/+$/, "")}/web-records-${process.pid}`, username: process.env.AIREADER_SYNC_USER ?? "", password: process.env.AIREADER_SYNC_PASSWORD ?? "" };
  const folder = syncPartsUrl(settings);
  // Counts the record files a round reads and sends.
  const pass = async (env) => {
    const counts = { read: 0, sent: 0 };
    const fetch = globalThis.fetch;
    globalThis.fetch = async (url, options) => {
      const response = await fetch(url, options);
      if (String(url).startsWith(folder) && response.ok) {
        if (options.method === "GET") counts.read++;
        if (options.method === "PUT") counts.sent++;
      }
      return response;
    };
    try {
      const outcome = await syncRecords(env, settings);
      return { ...counts, ...outcome, error: "" };
    } catch (error) {
      return { ...counts, error: error.message };
    } finally {
      globalThis.fetch = fetch;
    }
  };
  const lookup = (word, at) => lookupRecord({ word, sentence: "s", lemma: word, meaning: "m", lookedUpAt: at, updatedAt: at });
  const old = {
    books: [bookRecord({ key: "le grand meaulnes|alain-fournier", title: "Le Grand Meaulnes", group: "Série", chapter: 2, fraction: 0.5, snippet: "x", updatedAt: "2026-09-16T10:00:00Z" })],
    lookups: [lookup("un", "2026-09-16T10:00:00Z"), lookup("deux", "2026-09-16T10:00:00Z")],
  };
  await upload(syncOldFileUrl(settings), encodeDocument(old), settings);

  const a = await freshEnv();
  const id = await a.library.add({ title: "Le Grand Meaulnes", author: "Alain-Fournier", language: "fr" }, new Blob(["epub"]), null);
  const moved = await pass(a);
  const book = await a.library.find(id);
  check("the first sync moves the old file over", !moved.error && moved.sent === splitDocument(old).length && (await a.lookups.all()).length === 2
    && book.groupId && book.place?.chapter === 2, `${moved.error} ${moved.sent} sent`);
  const quiet = await pass(a);
  check("then a sync reads and sends nothing", !quiet.error && quiet.read === 0 && quiet.sent === 0, `${quiet.error} ${quiet.read} read`);

  const b = await freshEnv();
  const joined = await pass(b);
  check("a second device receives the records and sends nothing", !joined.error && joined.sent === 0 && (await b.lookups.all()).length === 2, joined.error);
  await b.cards.record((await b.lookups.all()).find((record) => record.word === "un").id, true);
  const practised = await pass(b);
  check("a change there sends only its file", !practised.error && practised.read === 1 && practised.sent === 1,
    `${practised.error} ${practised.read} read, ${practised.sent} sent`);
  const received = await pass(a);
  const card = (await a.cards.all()).find((item) => item.front === "un");
  check("and the first device reads only that file", !received.error && received.read === 1 && received.sent === 0 && card?.correct === 1,
    `${received.error} ${received.read} read, ${received.sent} sent`);

  // An older version on a third device writes the old file again.
  old.lookups.push(lookup("trois", "2026-09-17T10:00:00Z"));
  await upload(syncOldFileUrl(settings), encodeDocument(old), settings);
  const passed = await pass(a);
  check("a change to the old file still comes through", !passed.error && (await a.lookups.all()).length === 3 && passed.sent === 1
    && passed.applied.lookups === 1, `${passed.error} ${passed.sent} sent`);
  check("and is read once", (await pass(a)).read === 0);
}
