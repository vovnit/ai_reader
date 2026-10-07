// The calls of `src/Support/Idb.js`, answered from memory, so the stores can
// be checked under Node, which has no IndexedDB. Built from the same schema.

/** IndexedDB's key order: numbers, then strings, then arrays, element by element. */
function compare(a, b) {
  const rank = (key) => (typeof key === "number" ? 0 : typeof key === "string" ? 1 : 2);
  if (rank(a) !== rank(b)) return rank(a) - rank(b);
  if (Array.isArray(a)) {
    for (let i = 0; i < Math.min(a.length, b.length); i++) {
      const order = compare(a[i], b[i]);
      if (order) return order;
    }
    return a.length - b.length;
  }
  return a < b ? -1 : a > b ? 1 : 0;
}

const valid = (key) => typeof key === "number" || typeof key === "string" || (Array.isArray(key) && key.every(valid));
const read = (value, path) => (Array.isArray(path) ? path.map((part) => value[part]) : value[path]);

export class MemoryDatabase {
  #stores = new Map();

  static async open(schema) {
    const database = new MemoryDatabase();
    const raw = {
      createObjectStore: (name, options = {}) => {
        const store = { ...options, indexes: new Map(), rows: new Map(), next: 1 };
        database.#stores.set(name, store);
        return {
          createIndex: (index, keyPath, indexOptions = {}) => store.indexes.set(index, { keyPath, ...indexOptions }),
          put: (value) => database.put(name, value),
        };
      },
    };
    for (const migration of schema.migrations) migration(raw);
    return database;
  }

  #store(name) {
    const store = this.#stores.get(name);
    if (!store) throw new Error(`No store ${name}`);
    return store;
  }

  #sorted(store) {
    return [...store.rows.values()].sort((a, b) => compare(a.key, b.key));
  }

  async get(name, key) {
    return this.#store(name).rows.get(JSON.stringify(key))?.value ?? null;
  }

  async getAll(name, index, key) {
    const store = this.#store(name);
    const rows = this.#sorted(store);
    if (!index) return rows.map((row) => row.value);
    const { keyPath } = store.indexes.get(index);
    return rows.filter((row) => JSON.stringify(read(row.value, keyPath)) === JSON.stringify(key)).map((row) => row.value);
  }

  async getByIndex(name, index, key) {
    return (await this.getAll(name, index, key))[0] ?? null;
  }

  async put(name, value, explicitKey) {
    const store = this.#store(name);
    let key = explicitKey;
    let stored = value;
    if (store.keyPath) {
      key = read(value, store.keyPath);
      if (key === undefined && store.autoIncrement) {
        key = store.next;
        stored = { ...value, [store.keyPath]: key };
      }
    }
    if (!valid(key)) throw new Error(`Invalid key for ${name}`);
    if (typeof key === "number" && store.autoIncrement) store.next = Math.max(store.next, key + 1);
    for (const [index, { keyPath, unique }] of store.indexes) {
      const indexKey = JSON.stringify(read(stored, keyPath));
      const clash = unique && [...store.rows.values()].some((row) => JSON.stringify(read(row.value, keyPath)) === indexKey && compare(row.key, key));
      if (clash) throw new Error(`Index ${index} of ${name} refuses a duplicate`);
    }
    store.rows.set(JSON.stringify(key), { key, value: typeof stored === "object" && !(stored instanceof Blob) ? structuredClone(stored) : stored });
    return key;
  }

  async putAll(name, values) {
    for (const value of values) await this.put(name, value);
  }

  async delete(name, key) {
    this.#store(name).rows.delete(JSON.stringify(key));
  }

  async keysInRange(name, lower, upper) {
    return this.#sorted(this.#store(name)).map((row) => row.key).filter((key) => compare(key, lower) >= 0 && compare(key, upper) <= 0);
  }

  async deleteRange(name, lower, upper) {
    const store = this.#store(name);
    for (const [id, row] of store.rows) if (compare(row.key, lower) >= 0 && compare(row.key, upper) <= 0) store.rows.delete(id);
  }
}

/** `localStorage`'s calls, for the settings. */
export class MemoryStorage {
  #items = new Map();
  getItem(key) {
    return this.#items.get(key) ?? null;
  }
  setItem(key, value) {
    this.#items.set(key, String(value));
  }
}
