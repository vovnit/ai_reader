// The books on the shelf. A book is
// `{ id, title, author, language, addedAt, readingChapter, readingOffset,
//    groupId, place, placePending, updatedAt, remoteName }`:
// where it was last read as a chapter and an offset, and the same `place`
// in the form another device can use — pending when it came from one and
// the offset has not been worked out yet, which the reader does on opening.
// `updatedAt` is empty for a book never read here, which any other
// device's record outranks. Its EPUB and cover are kept beside it.
import { now } from "../Support/Text.js";

const newestFirst = (a, b) => (a.addedAt < b.addedAt ? 1 : a.addedAt > b.addedAt ? -1 : b.id - a.id);

export class LibraryStore {
  #db;

  constructor(db) {
    this.#db = db;
  }

  async all() {
    return (await this.#db.getAll("books")).sort(newestFirst);
  }

  find(id) {
    return this.#db.get("books", id);
  }

  /** The books of a group, in the order they joined the shelf. */
  async inGroup(groupId) {
    return (await this.all()).filter((book) => book.groupId === groupId).reverse();
  }

  /** Shelves a book with its EPUB and, if it has one, its cover. Returns its id. */
  async add({ title, author, language, remoteName = "" }, file, cover) {
    const id = await this.#db.put("books", {
      title, author, language, addedAt: now(), readingChapter: 0, readingOffset: 0,
      groupId: 0, place: null, placePending: false, updatedAt: "", remoteName,
    });
    await this.#db.put("bookFiles", file, id);
    if (cover) await this.#db.put("covers", cover, id);
    // A browser may clear a site's storage when space runs short, unless
    // asked to keep it; a library is worth asking for.
    globalThis.navigator?.storage?.persist?.().catch(() => {});
    return id;
  }

  /** Takes the book off the shelf; words looked up in it are kept, belonging to no book. */
  async remove(id) {
    await this.#db.delete("books", id);
    await this.#db.delete("bookFiles", id);
    await this.#db.delete("covers", id);
    for (const lookup of await this.#db.getAll("lookups", "bookId", id)) {
      await this.#db.put("lookups", { ...lookup, bookId: null });
    }
  }

  file(id) {
    return this.#db.get("bookFiles", id);
  }

  cover(id) {
    return this.#db.get("covers", id);
  }

  async #update(id, changes) {
    const book = await this.find(id);
    if (book) await this.#db.put("books", { ...book, ...changes });
  }

  savePosition(id, chapter, offset, place) {
    return this.#update(id, { readingChapter: chapter, readingOffset: offset, place, placePending: false, updatedAt: now() });
  }

  /** A place from another device, to be found in the text when the book is next opened. */
  savePendingPlace(id, place, updatedAt) {
    return this.#update(id, { readingChapter: place.chapter, readingOffset: 0, place, placePending: true, updatedAt });
  }

  /** What the prose says the book is written in; past lookups were recorded under the old language and are corrected too. */
  async saveLanguage(id, language) {
    await this.#update(id, { language });
    for (const lookup of await this.#db.getAll("lookups", "bookId", id)) {
      await this.#db.put("lookups", { ...lookup, language });
    }
  }

  /** Puts the book in a group; 0 takes it out. */
  assignGroup(id, groupId, updatedAt = "") {
    return this.#update(id, { groupId, updatedAt: updatedAt || now() });
  }

  setRemoteName(id, name) {
    return this.#update(id, { remoteName: name });
  }

  /** The files in the sync folder's `Books`, as the last sync listed them; those no book here has as its `remoteName` can be fetched on request. */
  async remoteNames() {
    return new Set((await this.#db.getAll("remoteBooks")).map((row) => row.name));
  }

  async setRemoteNames(names) {
    const listed = new Set(names);
    for (const name of await this.remoteNames()) if (!listed.has(name)) await this.#db.delete("remoteBooks", name);
    await this.#db.putAll("remoteBooks", names.map((name) => ({ name })));
  }

  forgetRemote(name) {
    return this.#db.delete("remoteBooks", name);
  }
}
