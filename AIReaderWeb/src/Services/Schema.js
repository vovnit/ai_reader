// What the browser keeps, in IndexedDB. Migrations are additive and run in
// order, so a library made by an earlier version survives an update: add a
// function to the end, never change one that has shipped.
import { now } from "../Support/Text.js";

export const schema = {
  name: "aireader",
  migrations: [
    (db) => {
      // A book's record; its EPUB and cover sit apart, keyed by the book's
      // id, so listing the shelf does not read every file.
      db.createObjectStore("books", { keyPath: "id", autoIncrement: true });
      db.createObjectStore("bookFiles");
      db.createObjectStore("covers");
      db.createObjectStore("groups", { keyPath: "id", autoIncrement: true });

      // Every lookup, doubling as the cache: one per word and sentence.
      const lookups = db.createObjectStore("lookups", { keyPath: "id", autoIncrement: true });
      lookups.createIndex("wordSentence", ["word", "sentence"], { unique: true });
      lookups.createIndex("bookId", "bookId");
      // How each lookup has fared as a flash card, and the lookups that
      // were deleted, by name, so a sync deletes them elsewhere too.
      db.createObjectStore("cardPractice", { keyPath: "lookupId" });
      db.createObjectStore("lookupTombstones", { keyPath: ["word", "sentence"] });
      // The files in the sync folder's `Books`, as the last sync listed
      // them. (While sync fetched every book: every file this device had
      // met; the next sync replaces those with the listing.)
      db.createObjectStore("remoteBooks", { keyPath: "name" });

      // The dictionaries: SQLite packs kept as files, and the articles of
      // the ones converted from other formats, by headword.
      const packs = db.createObjectStore("dictionaryPacks", { keyPath: "id", autoIncrement: true });
      db.createObjectStore("dictionaryFiles");
      db.createObjectStore("dictionaryArticles", { keyPath: ["packId", "word"] });
      // The pack that ships with the app, so it appears beside any added.
      packs.put({
        name: "Bundled dictionary",
        kind: "bundled",
        fileName: "",
        targetLanguage: "fr",
        definitionLanguage: "ru",
        isEnabled: true,
        addedAt: now(),
      });
    },
    (db) => {
      // What this device last knew of each file in the sync folder's
      // `aireader-sync`: `{ folder, name, version, digest }`, the server's
      // version of it and a fingerprint of the records here that belong in
      // it, so a sync reads and writes only the files that changed.
      db.createObjectStore("syncFiles", { keyPath: ["folder", "name"] });
    },
  ],
};
