// The sync document and the store that turns the library into one and back
// — the Kindle check's cases, with the same expectations.
import {
  bookRecord, encodeDocument, lookupRecord, mergeDocuments, parseDocument, sameDocument,
} from "../../src/Domain/Sync/SyncDocument.js";
import { applyDocument, exportAll } from "../../src/Services/SyncStore.js";
import { fileNames, escapeName } from "../../src/Services/WebDav.js";
import { check } from "./Check.mjs";
import { freshEnv } from "./Env.mjs";

const book = (key, at, group = "", chapter = 1) =>
  bookRecord({ key, title: key, language: "fr", group, chapter, fraction: 0.5, snippet: "x", updatedAt: at });
const lookup = (word, at, bookKey = "", deleted = false, correct = 0) =>
  lookupRecord({ word, sentence: "s", lemma: word, meaning: "m", book: bookKey, lookedUpAt: at, correct, updatedAt: at, deleted });

export async function checkSync() {
  const parsed = parseDocument(`{"version":1,"books":[{"author":null,"chapter":2,"fraction":0.25,"group":"Série","key":"a|","language":"fr","snippet":"Il vint","title":"A","updatedAt":"2026-09-16T10:00:00Z"}],
    "lookups":[{"book":null,"confidence":0.9,"correct":1,"deleted":false,"formNote":"","guessed":false,"language":"fr","lemma":"un","lookedUpAt":"2026-09-16T09:00:00Z","meaning":"один","practicedAt":"2026-09-16T10:00:00Z","sentence":"s","updatedAt":"2026-09-16T10:00:00Z","word":"un","wrong":0}]}`);
  check("sync document parses", parsed?.books.length === 1 && parsed.lookups.length === 1);
  check("sync document reads a place and a group", parsed.books[0].chapter === 2 && parsed.books[0].fraction === 0.25 && parsed.books[0].snippet === "Il vint" && parsed.books[0].group === "Série");
  check("sync document reads practice", parsed.lookups[0].correct === 1 && parsed.lookups[0].practicedAt === "2026-09-16T10:00:00Z" && parsed.lookups[0].book === "");
  const again = parseDocument(encodeDocument(parsed));
  check("sync document round-trips", again && sameDocument(again, parsed), encodeDocument(parsed));
  check("absent values are written as null", encodeDocument(parsed).includes('"author":null') && encodeDocument(parsed).includes('"book":null'));
  check("members in the C++ app's order", encodeDocument(parsed).startsWith('{"books":[{"author":null,"chapter":2,"fraction":0.25,"group":"Série","key":"a|"'));

  const local = {
    books: [book("a|", "2026-09-16T10:00:00Z", "", 2), book("b|", "2026-09-16T09:00:00Z", "Série")],
    lookups: [lookup("un", "2026-09-16T10:00:00Z", "", false, 3), lookup("deux", "2026-09-16T08:00:00Z"), lookup("trois", "2026-09-16T11:00:00Z", "", true)],
  };
  const remote = {
    books: [book("a|", "2026-09-16T09:00:00Z", "", 1), book("c|", "2026-09-16T09:00:00Z")],
    lookups: [lookup("un", "2026-09-16T09:00:00Z", "b|"), lookup("deux", "2026-09-16T09:00:00Z", "", true), lookup("trois", "2026-09-16T12:00:00Z"), lookup("quatre", "2026-09-16T09:00:00Z")],
  };
  const merged = mergeDocuments(local, remote);
  const books = new Map(merged.books.map((record) => [record.key, record]));
  const words = new Map(merged.lookups.map((record) => [record.word, record]));
  check("newer book record wins", books.get("a|").chapter === 2 && books.size === 3);
  check("newer lookup wins and keeps the other side's book", words.get("un").correct === 3 && words.get("un").book === "b|");
  check("a newer tombstone deletes", words.get("deux").deleted);
  check("a lookup made again after deletion comes back", !words.get("trois").deleted);
  check("unknown records are kept", words.has("quatre") && merged.lookups.length === 4);
  check("merge is deterministic", sameDocument(mergeDocuments(remote, local), merged));

  const env = await freshEnv();
  const draft = { title: "Le Grand Meaulnes", author: "Alain-Fournier", language: "fr" };
  const id = await env.library.add(draft, new Blob(["epub"]), null);
  check("a book never read here has no date, so any other device's record wins",
    (await exportAll(env)).books[0].updatedAt === ""
    && mergeDocuments(await exportAll(env), { books: [book("le grand meaulnes|alain-fournier", "2000-01-01T00:00:00Z")], lookups: [] }).books[0].updatedAt === "2000-01-01T00:00:00Z");
  await env.library.savePosition(id, 1, 10, { chapter: 1, fraction: 0.1, snippet: "Il vint" });
  await env.lookups.save({ word: "maisons", sentence: "Les maisons.", language: "fr", bookId: id }, { lemma: "maison", formNote: "pl.", meaning: "дом", guessed: false, confidence: 0.9 });
  const lookupId = (await env.lookups.all())[0].id;
  await env.cards.record(lookupId, true);
  const exported = await exportAll(env);
  check("export names the book by title and author", exported.books.length === 1 && exported.books[0].key === "le grand meaulnes|alain-fournier"
    && exported.books[0].chapter === 1 && exported.books[0].snippet === "Il vint" && exported.books[0].updatedAt !== "");
  check("export carries the lookup with its practice and book", exported.lookups.length === 1 && exported.lookups[0].book === exported.books[0].key
    && exported.lookups[0].correct === 1 && exported.lookups[0].updatedAt === exported.lookups[0].practicedAt);

  const read = { ...exported.books[0], group: "Série", chapter: 3, fraction: 0.4, snippet: "Plus loin", updatedAt: "2999-01-01T00:00:00Z" };
  const practised = { ...exported.lookups[0], correct: 5, practicedAt: "2999-01-01T00:00:00Z", updatedAt: "2999-01-01T00:00:00Z" };
  const fresh = { ...lookup("chat", "2999-01-01T00:00:00Z", read.key), sentence: "Le chat." };
  const gone = lookup("chien", "2999-01-01T00:00:00Z", "", true);
  const mergedStore = mergeDocuments(exported, { books: [read], lookups: [practised, fresh, gone] });
  const applied = await applyDocument(env, mergedStore);
  const stored = await env.library.find(id);
  check("applying moves the book into the group", applied.books === 1 && stored.groupId && (await env.groups.find(stored.groupId)).name === "Série");
  check("applying leaves the place pending for the reader", stored.placePending && stored.readingChapter === 3 && stored.place.snippet === "Plus loin");
  const cards = await env.cards.all();
  check("applying writes the practice and the new word", applied.lookups === 3 && cards.length === 2 && cards[0].front === "chat" && cards[1].correct === 5, JSON.stringify(cards.map((card) => card.front)));
  check("applying records the deletion", (await env.lookups.tombstones()).length === 1 && (await env.lookups.tombstones())[0].word === "chien");
  check("the new word belongs to the book here", (await env.lookups.all(id)).length === 2);
  check("a second sync has nothing to apply", sameDocument(await exportAll(env), mergedStore) && (await applyDocument(env, mergedStore)).lookups === 0);
  await env.lookups.remove((await env.lookups.all(id))[0].id);
  check("a deletion here becomes a tombstone", (await env.lookups.tombstones()).length === 2);
  const after = await exportAll(env);
  check("tombstones are exported", after.lookups.filter((record) => record.deleted).length === 2 && after.lookups.length === 3);

  const listing = '<?xml version="1.0"?><D:multistatus xmlns:D="DAV:">'
    + "<D:response><D:href>/dav/Books/</D:href><D:propstat><D:prop><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat></D:response>"
    + "<D:response><D:href>/dav/Books/Saint-Exup%C3%A9ry%20-%20Vol%20de%20nuit.epub</D:href><D:propstat><D:prop><D:resourcetype/></D:prop></D:propstat></D:response>"
    + "<D:response><D:href>https://example.org/dav/Books/Old/</D:href><D:propstat><D:prop><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat></D:response>"
    + "</D:multistatus>";
  const names = fileNames(listing);
  check("listing gives files, decoded, without folders", names.length === 1 && names[0] === "Saint-Exupéry - Vol de nuit.epub", names.join());
  check("names escape for a url", escapeName("Vol de nuit é.epub") === "Vol%20de%20nuit%20%C3%A9.epub");
}
