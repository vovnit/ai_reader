// The shelf: the books, the groups they are sorted into, the books in the
// sync folder not fetched yet, and the door into the reader.
import { bookKey } from "../../Domain/Books/BookKey.js";
import { epubCover, epubMetadata } from "../../Services/EpubLoader.js";
import { deleteRemoteBook, fetchBook } from "../../Services/LibrarySync.js";
import { pdfToEpub } from "../../Services/PdfImporter.js";
import { syncConfigured } from "../../Services/Settings.js";
import { runSync } from "../../Services/Sync.js";
import { Feature } from "../Common/Feature.js";

const isPdf = (file) => file.type === "application/pdf" || /\.pdf$/i.test(file.name);
/** A file in the sync folder as the shelf shows it: author and title, without `.epub`. */
export const remoteTitle = (name) => name.replace(/\.epub$/i, "");

export class LibraryFeature extends Feature {
  books = [];
  groups = [];
  /** The files in the sync folder no book here has, by name; none when no server is set up. */
  cloudBooks = [];
  /** The files being fetched from the sync folder. */
  downloading = new Set();
  isAdding = false;
  /** What went wrong adding, fetching or deleting books, if anything did. */
  message = "";
  #remote = new Set();
  #env;
  #syncing = false;

  constructor(env) {
    super();
    this.#env = env;
  }

  async reload() {
    this.books = await this.#env.library.all();
    this.groups = await this.#env.groups.all();
    this.#remote = syncConfigured(this.#env.settings.sync()) ? await this.#env.library.remoteNames() : new Set();
    const here = new Set(this.books.map((book) => book.remoteName));
    this.cloudBooks = [...this.#remote].filter((name) => !here.has(name)).sort();
    this.changed();
  }

  /** Whether the book's file is in the sync folder. */
  isInCloud(book) {
    return Boolean(book.remoteName) && this.#remote.has(book.remoteName);
  }

  /** The books in a group; for 0, the books in none. */
  booksIn(groupId) {
    return this.books.filter((book) => book.groupId === groupId);
  }

  /** Puts the EPUBs on the shelf, a PDF made into one first; returns the ids of the ones added. */
  async add(files) {
    this.isAdding = true;
    this.message = "";
    this.changed();
    const problems = [];
    const added = [];
    const keys = new Set(this.books.map((book) => bookKey(book.title, book.author)));
    for (const picked of files) {
      try {
        const file = isPdf(picked)
          ? await pdfToEpub(new Uint8Array(await picked.arrayBuffer()), picked.name.replace(/\.pdf$/i, ""))
          : picked;
        const metadata = await epubMetadata(file, picked.name);
        const key = bookKey(metadata.title, metadata.author);
        if (keys.has(key)) {
          problems.push(`“${metadata.title}” is already on the shelf.`);
          continue;
        }
        keys.add(key);
        added.push(await this.#env.library.add(metadata, file, await epubCover(file).catch(() => null)));
      } catch (error) {
        problems.push(`${picked.name}: ${error.message}`);
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

  /** Deletes the book's file from the sync folder, then takes the book off this device. */
  async removeEverywhere(book) {
    // The file first: if it cannot go, the book stays as it was.
    if (await this.#attempt(`Couldn’t delete “${book.title}”`, () => deleteRemoteBook(this.#env, this.#env.settings.sync(), book.remoteName))) {
      await this.remove(book);
    }
  }

  /** Deletes a file from the sync folder; devices that have the book keep their copy. */
  async removeRemote(name) {
    await this.#attempt(`Couldn’t delete “${remoteTitle(name)}”`, () => deleteRemoteBook(this.#env, this.#env.settings.sync(), name));
    await this.reload();
  }

  /** Fetches a file from the sync folder onto the shelf. */
  async download(name) {
    if (this.downloading.has(name)) return;
    this.downloading.add(name);
    this.changed();
    const fetched = await this.#attempt(`Couldn’t download “${remoteTitle(name)}”`, () => fetchBook(this.#env, this.#env.settings.sync(), name));
    this.downloading.delete(name);
    await this.reload();
    // Where another device is in it, and its group, come with the document.
    if (fetched) await this.sync();
  }

  /** Runs `work`; true when it went through, else says why. */
  async #attempt(title, work) {
    this.message = "";
    try {
      await work();
      return true;
    } catch (error) {
      this.message = `${title}: ${error.message}`;
      this.changed();
      return false;
    }
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
