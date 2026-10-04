// Against a real WebDAV server, when `AIREADER_SYNC_URL` (and `_USER`,
// `_PASSWORD`) name one: three devices share a book through the folder, and
// a round trip of the sync file. Node sends no Origin, so this checks the
// protocol, not the server's CORS; the browser does that.
import { runSync, fetchDocument } from "../../src/Services/Sync.js";
import { exchangeBooks } from "../../src/Services/LibrarySync.js";
import { check, skip } from "./Check.mjs";
import { freshEnv } from "./Env.mjs";
import { epubMetadata } from "../../src/Services/EpubLoader.js";

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
  check("first device sends its book", !sent.error && sent.sent === 1 && sent.received === 0, sent.error);
  const quiet = await exchangeBooks(a, settings);
  check("first device then has nothing to do", !quiet.error && quiet.sent === 0 && quiet.received === 0, quiet.error);

  const b = await device();
  const fetched = await exchangeBooks(b, settings);
  const shelf = await b.library.all();
  check("second device fetches it", !fetched.error && fetched.received === 1 && shelf.length === 1 && shelf[0].remoteName
    && (await b.library.file(shelf[0].id)).size === book.size, fetched.error);
  await b.library.remove(shelf[0].id);
  const removed = await exchangeBooks(b, settings);
  check("a book removed here is not fetched again", removed.received === 0 && !(await b.library.all()).length);

  const c = await device();
  await c.library.add(await epubMetadata(book), book, null);
  const matched = await exchangeBooks(c, settings);
  check("a book already here is recognised, not fetched or sent", !matched.error && matched.received === 0 && matched.sent === 0
    && (await c.library.all())[0].remoteName === (await a.library.find(id)).remoteName, matched.error);

  await a.library.savePosition(id, 1, 10, { chapter: 1, fraction: 0.1, snippet: "Il vint" });
  await a.lookups.save({ word: "maisons", sentence: "Les maisons.", language: "fr", bookId: id }, { lemma: "maison", formNote: "pl.", meaning: "дом", guessed: false, confidence: 0.9 });
  const first = await runSync(a);
  check("sync server round trip", first && !first.failed && (await fetchDocument(settings)).lookups.length === 1, first?.message);
  const second = await runSync(a);
  check("second sync has nothing to send", second?.message.endsWith("the server was up to date."), second?.message);
  const other = await runSync(c);
  const place = (await c.library.all())[0];
  check("the other device takes the word and the place", !other.failed && (await c.lookups.all()).length === 1 && place.placePending && place.place.snippet === "Il vint", other.message);
  console.log(`      ${first.message}`);
}
