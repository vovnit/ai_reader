// The entry point: opens the library, puts the shelf on screen, takes in
// books opened with the installed app, syncs, and keeps the app for
// offline use.
import { Navigator } from "../Features/Common/Navigator.js";
import { LibraryView } from "../Features/Library/LibraryView.js";
import { schema } from "../Services/Schema.js";
import { SettingsStore } from "../Services/Settings.js";
import { IdbDatabase } from "../Support/Idb.js";
import { makeEnv } from "./Env.js";

let dictionary = null;

/** The bundled dictionary, fetched once a session; the service worker keeps it for offline use. */
function bundledDictionary() {
  dictionary ??= fetch("dictionary.sqlite3").then((response) => {
    if (!response.ok) throw new Error(`The bundled dictionary could not be loaded (${response.status}).`);
    return response.blob();
  });
  dictionary.catch(() => (dictionary = null));
  return dictionary;
}

async function start() {
  const db = await IdbDatabase.open(schema);
  const env = makeEnv({ db, storage: localStorage, run: SettingsStore.runValues(location.search), bundled: bundledDictionary });
  const screens = new Navigator(document.getElementById("main"), document.getElementById("panel"));
  const library = new LibraryView(env, screens);
  screens.push(library);

  // Books opened with the installed app from the system's files.
  window.launchQueue?.setConsumer(async ({ files }) => {
    const opened = await Promise.all(files.map((handle) => handle.getFile()));
    if (opened.length) await library.receive(opened);
  });
  library.feature.sync();
  if ("serviceWorker" in navigator) navigator.serviceWorker.register("sw.js").catch((error) => console.warn(error));
  bundledDictionary().catch((error) => console.warn(error.message));
}

start().catch((error) => {
  document.getElementById("main").textContent = `AIReader could not start: ${error.message}`;
});
