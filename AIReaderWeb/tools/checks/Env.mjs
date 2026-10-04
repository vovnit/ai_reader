// A library in memory, as the checks need one: the app's own stores over
// the stand-in database, and the bundled dictionary read from disk.
import { openAsBlob } from "node:fs";
import { fileURLToPath } from "node:url";
import { makeEnv } from "../../src/App/Env.js";
import { schema } from "../../src/Services/Schema.js";
import { MemoryDatabase, MemoryStorage } from "../MemoryDatabase.js";

export const dictionaryPath = fileURLToPath(new URL("../../dictionary.sqlite3", import.meta.url));

export async function freshEnv() {
  return makeEnv({
    db: await MemoryDatabase.open(schema),
    storage: new MemoryStorage(),
    bundled: () => openAsBlob(dictionaryPath),
  });
}
