// Everything a feature reaches the outside world through, made once: the
// stores over the database, the dictionaries and the settings.
import { CardStore } from "../Services/CardStore.js";
import { DictionaryDatabase } from "../Services/DictionaryDatabase.js";
import { DictionaryPacks } from "../Services/DictionaryPacks.js";
import { GroupStore } from "../Services/GroupStore.js";
import { LibraryStore } from "../Services/LibraryStore.js";
import { LookupCache } from "../Services/LookupCache.js";
import { SettingsStore } from "../Services/Settings.js";
import { SyncFileStore } from "../Services/SyncFileStore.js";

/** `db` is an `IdbDatabase` (or its stand-in), `bundled` gives the bundled dictionary's Blob. */
export function makeEnv({ db, storage, run = {}, bundled }) {
  const lookups = new LookupCache(db);
  const dictionary = new DictionaryDatabase(db, bundled);
  return {
    library: new LibraryStore(db),
    groups: new GroupStore(db),
    lookups,
    cards: new CardStore(db, lookups),
    dictionary,
    packs: new DictionaryPacks(db, dictionary),
    settings: new SettingsStore(storage, run),
    syncFiles: new SyncFileStore(db),
  };
}

/** What the model may reach for while answering: the books in `scope`, the enabled dictionaries, the web. */
export async function toolsFor(env, scope = {}) {
  return { scope, packs: await env.packs.enabled(), web: env.settings.web(), dictionary: env.dictionary };
}
