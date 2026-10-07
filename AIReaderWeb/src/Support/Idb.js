// IndexedDB with promises, one transaction per call: the handful of
// operations the stores need. `tools/MemoryDatabase.js` answers the same
// calls from memory, so the stores can be checked under Node.

function done(request) {
  return new Promise((resolve, reject) => {
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

function finished(transaction) {
  return new Promise((resolve, reject) => {
    transaction.oncomplete = () => resolve();
    transaction.onerror = () => reject(transaction.error);
    transaction.onabort = () => reject(transaction.error ?? new Error("The database refused the change."));
  });
}

export class IdbDatabase {
  #db;

  constructor(db) {
    this.#db = db;
  }

  /**
   * Opens the database, running `schema.migrations[n]` for every version
   * above the one on disk; each gets the raw database and the upgrade
   * transaction.
   */
  static async open(schema) {
    const request = indexedDB.open(schema.name, schema.migrations.length);
    request.onupgradeneeded = (event) => {
      for (let version = event.oldVersion; version < schema.migrations.length; version++) {
        schema.migrations[version](request.result, request.transaction);
      }
    };
    return new IdbDatabase(await done(request));
  }

  async #run(store, mode, operate) {
    const transaction = this.#db.transaction(store, mode);
    const result = done(operate(transaction.objectStore(store)));
    await finished(transaction);
    return result;
  }

  get(store, key) {
    return this.#run(store, "readonly", (objects) => objects.get(key)).then((value) => value ?? null);
  }

  /** Every record in key order, or those whose `index` equals `key`. */
  getAll(store, index, key) {
    return this.#run(store, "readonly", (objects) => (index ? objects.index(index).getAll(key) : objects.getAll()));
  }

  /** The first record whose `index` equals `key`, or null. */
  getByIndex(store, index, key) {
    return this.#run(store, "readonly", (objects) => objects.index(index).get(key)).then((value) => value ?? null);
  }

  /** Stores the value; `key` for stores without a key path. Returns the key. */
  put(store, value, key) {
    return this.#run(store, "readwrite", (objects) => (key === undefined ? objects.put(value) : objects.put(value, key)));
  }

  /** Many values in one transaction, which is far faster than one each. */
  async putAll(store, values) {
    const transaction = this.#db.transaction(store, "readwrite");
    const objects = transaction.objectStore(store);
    for (const value of values) objects.put(value);
    await finished(transaction);
  }

  delete(store, key) {
    return this.#run(store, "readwrite", (objects) => objects.delete(key));
  }

  /** The keys from `lower` to `upper`, both included, in order. */
  keysInRange(store, lower, upper) {
    return this.#run(store, "readonly", (objects) => objects.getAllKeys(IDBKeyRange.bound(lower, upper)));
  }

  /** Deletes the records with keys from `lower` to `upper`, both included. */
  deleteRange(store, lower, upper) {
    return this.#run(store, "readwrite", (objects) => objects.delete(IDBKeyRange.bound(lower, upper)));
  }
}
