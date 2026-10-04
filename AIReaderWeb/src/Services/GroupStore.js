// Groups of books read together — a series, a course — so a search runs
// across them: `{ id, name, createdAt }`. A book leaves its group when the
// group goes.
import { now } from "../Support/Text.js";

export class GroupStore {
  #db;

  constructor(db) {
    this.#db = db;
  }

  async all() {
    const groups = await this.#db.getAll("groups");
    return groups.sort((a, b) => a.name.localeCompare(b.name, undefined, { sensitivity: "accent" }) || a.id - b.id);
  }

  find(id) {
    return this.#db.get("groups", id);
  }

  /** The group of that name, made if there is none yet. */
  async named(name) {
    const existing = (await this.#db.getAll("groups")).find((group) => group.name === name);
    if (existing) return existing.id;
    return this.#db.put("groups", { name, createdAt: now() });
  }

  /** Dissolves the group; its books stay, in none. */
  async remove(id) {
    await this.#db.delete("groups", id);
    for (const book of await this.#db.getAll("books")) {
      if (book.groupId === id) await this.#db.put("books", { ...book, groupId: 0 });
    }
  }
}
