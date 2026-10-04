// The shelf: the books, the groups they are sorted into, and the door into
// the reader.
import { bookKey } from "../../Domain/Books/BookKey.js";
import { epubCover, epubMetadata } from "../../Services/EpubLoader.js";
import { runSync } from "../../Services/Sync.js";
import { Feature } from "../Common/Feature.js";

export class LibraryFeature extends Feature {
  books = [];
  groups = [];
  isAdding = false;
  /** What went wrong adding books, if anything did. */
  message = "";
  #env;
  #syncing = false;

  constructor(env) {
    super();
    this.#env = env;
  }

  async reload() {
    this.books = await this.#env.library.all();
    this.groups = await this.#env.groups.all();
    this.changed();
  }

  /** The books in a group; for 0, the books in none. */
  booksIn(groupId) {
    return this.books.filter((book) => book.groupId === groupId);
  }

  /** Puts the EPUBs on the shelf; returns the ids of the ones added. */
  async add(files) {
    this.isAdding = true;
    this.message = "";
    this.changed();
    const problems = [];
    const added = [];
    const keys = new Set(this.books.map((book) => bookKey(book.title, book.author)));
    for (const file of files) {
      try {
        const metadata = await epubMetadata(file, file.name);
        const key = bookKey(metadata.title, metadata.author);
        if (keys.has(key)) {
          problems.push(`“${metadata.title}” is already on the shelf.`);
          continue;
        }
        keys.add(key);
        added.push(await this.#env.library.add(metadata, file, await epubCover(file).catch(() => null)));
      } catch (error) {
        problems.push(`${file.name}: ${error.message}`);
      }
    }
    this.isAdding = false;
    this.message = problems.join("\n");
    await this.reload();
    return added;
  }

  /** Takes the book off this device; its file stays in the sync folder for the others. */
  async remove(book) {
    await this.#env.library.remove(book.id);
    await this.reload();
  }

  /** Puts the book in a group, by id, or by a name for a new one; 0 takes it out. */
  async assign(book, group) {
    const id = typeof group === "string" ? await this.#env.groups.named(group.trim()) : group;
    await this.#env.library.assignGroup(book.id, id);
    await this.reload();
  }

  /** Dissolves the group; its books stay, ungrouped. */
  async dissolve(group) {
    await this.#env.groups.remove(group.id);
    await this.reload();
  }

  /** Brings this device in line with the others, when a server is set up. Quiet: what came in shows on the shelf; a failure goes to the console. */
  async sync() {
    if (this.#syncing) return;
    this.#syncing = true;
    const result = await runSync(this.#env);
    this.#syncing = false;
    if (result?.failed) console.warn(`AIReader sync: ${result.message}`);
    if (result) await this.reload();
  }
}
