// What this device last knew of each file in the sync folder's
// `aireader-sync` (`Domain/Sync/SyncParts.js`), kept with the folder's
// address, since another folder's versions say nothing.

export class SyncFileStore {
  #db;

  constructor(db) {
    this.#db = db;
  }

  /** The files of the folder at `folder`: `[{ name, version, digest }]`. */
  async known(folder) {
    return (await this.#db.getAll("syncFiles"))
      .filter((row) => row.folder === folder)
      .map(({ name, version, digest }) => ({ name, version, digest }));
  }

  /** Replaces them, and forgets any other folder's. */
  async remember(folder, files) {
    const kept = new Set(files.map((file) => file.name));
    for (const row of await this.#db.getAll("syncFiles")) {
      if (row.folder !== folder || !kept.has(row.name)) await this.#db.delete("syncFiles", [row.folder, row.name]);
    }
    await this.#db.putAll("syncFiles", files.map(({ name, version, digest }) => ({ folder, name, version, digest })));
  }
}
