// Keeps the app for offline use. Everything is fetched from the network
// when it can be, so a new version shows on the next load, and from the
// copy kept here when it cannot. The dictionary, large and rarely changed,
// comes from the copy first and is refreshed behind it.

const cacheName = "aireader";
const dictionary = "./dictionary.sqlite3";

// Every file of the app; `tools/check.mjs` fails when one is missing. The
// page is kept as "./" alone: hosts such as Cloudflare answer index.html with
// a redirect to "./", and a kept redirect cannot answer a page load offline.
const files = [
  "./",
  "./app.css",
  "./manifest.webmanifest",
  "./icon.svg",
  "./icon-192.png",
  "./icon-512.png",
  "./src/App/Env.js",
  "./src/App/main.js",
  "./src/Domain/AI/ChatMarkdown.js",
  "./src/Domain/AI/ChatMessage.js",
  "./src/Domain/AI/ChatPrompt.js",
  "./src/Domain/AI/ExplanationPrompt.js",
  "./src/Domain/AI/MockAI.js",
  "./src/Domain/AI/RequestQuirks.js",
  "./src/Domain/AI/SearchSummary.js",
  "./src/Domain/AI/Tools.js",
  "./src/Domain/AI/WordExplanation.js",
  "./src/Domain/AI/XRayPrompt.js",
  "./src/Domain/Books/BookKey.js",
  "./src/Domain/Books/Contents.js",
  "./src/Domain/Books/EpubNavigation.js",
  "./src/Domain/Books/EpubPackage.js",
  "./src/Domain/Books/HtmlText.js",
  "./src/Domain/Books/LanguageDetector.js",
  "./src/Domain/Books/ReadingPlace.js",
  "./src/Domain/Books/RemoteBookName.js",
  "./src/Domain/Cards/AnkiExport.js",
  "./src/Domain/Cards/Card.js",
  "./src/Domain/Cards/MatchRound.js",
  "./src/Domain/Dictionary/DictionaryLookup.js",
  "./src/Domain/Dictionary/WordNormalizer.js",
  "./src/Domain/Formats/DSLDictionaryReader.js",
  "./src/Domain/Formats/DelimitedDictionaryReader.js",
  "./src/Domain/Formats/DictionaryConverter.js",
  "./src/Domain/Formats/DictionaryFormat.js",
  "./src/Domain/Formats/StarDictReader.js",
  "./src/Domain/Formats/XDXFDictionaryReader.js",
  "./src/Domain/Reading/ChapterParagraphs.js",
  "./src/Domain/Reading/ReadingStyle.js",
  "./src/Domain/Reading/WordContext.js",
  "./src/Domain/Search/BookSearch.js",
  "./src/Domain/Sync/SyncDocument.js",
  "./src/Features/Chat/ChatFeature.js",
  "./src/Features/Chat/ChatView.js",
  "./src/Features/Common/Dialogs.js",
  "./src/Features/Common/Feature.js",
  "./src/Features/Common/Files.js",
  "./src/Features/Common/Navigator.js",
  "./src/Features/Common/Screen.js",
  "./src/Features/Common/Ui.js",
  "./src/Features/Contents/ContentsView.js",
  "./src/Features/Dictionaries/DictionariesFeature.js",
  "./src/Features/Dictionaries/DictionariesView.js",
  "./src/Features/Library/GroupView.js",
  "./src/Features/Library/LibraryFeature.js",
  "./src/Features/Library/LibraryView.js",
  "./src/Features/Lookup/EntryView.js",
  "./src/Features/Lookup/LookupFeature.js",
  "./src/Features/Lookup/LookupView.js",
  "./src/Features/Match/MatchFeature.js",
  "./src/Features/Match/MatchView.js",
  "./src/Features/Menu/DisplayFeature.js",
  "./src/Features/Menu/DisplayView.js",
  "./src/Features/Menu/MenuView.js",
  "./src/Features/Reader/PageFlow.js",
  "./src/Features/Reader/ReaderFeature.js",
  "./src/Features/Reader/ReaderView.js",
  "./src/Features/Search/SearchFeature.js",
  "./src/Features/Search/SearchView.js",
  "./src/Features/Settings/SettingsFeature.js",
  "./src/Features/Settings/SettingsView.js",
  "./src/Features/Words/WordsFeature.js",
  "./src/Features/Words/WordsView.js",
  "./src/Features/XRay/XRayFeature.js",
  "./src/Features/XRay/XRayView.js",
  "./src/Services/BookCorpus.js",
  "./src/Services/CardStore.js",
  "./src/Services/ChatApi.js",
  "./src/Services/DictionaryDatabase.js",
  "./src/Services/DictionaryPacks.js",
  "./src/Services/EpubLoader.js",
  "./src/Services/GroupStore.js",
  "./src/Services/Http.js",
  "./src/Services/LibrarySync.js",
  "./src/Services/LibraryStore.js",
  "./src/Services/LookupCache.js",
  "./src/Services/Schema.js",
  "./src/Services/Settings.js",
  "./src/Services/Speech.js",
  "./src/Services/Sync.js",
  "./src/Services/SyncStore.js",
  "./src/Services/ToolRunner.js",
  "./src/Services/WebDav.js",
  "./src/Services/WebSearch.js",
  "./src/Services/WordExplainer.js",
  "./src/Support/Idb.js",
  "./src/Support/Inflate.js",
  "./src/Support/SqliteFile.js",
  "./src/Support/Text.js",
  "./src/Support/TextFile.js",
  "./src/Support/XmlScanner.js",
  "./src/Support/ZipArchive.js",
];

self.addEventListener("install", (event) => {
  event.waitUntil(caches.open(cacheName).then((cache) => cache.addAll([...files, dictionary])));
  self.skipWaiting();
});

self.addEventListener("activate", (event) => event.waitUntil(self.clients.claim()));

async function fresh(request) {
  const cache = await caches.open(cacheName);
  const copy = () => cache.match(request, { ignoreSearch: true }).then((found) => found ?? (request.mode === "navigate" ? cache.match("./") : null));
  const network = fetch(request).then(async (response) => {
    if (response.ok) await cache.put(request, response.clone());
    return response;
  });
  // A connection that barely works is no better than none: after a few
  // seconds the copy answers, and the network still refreshes it.
  const slow = new Promise((resolve) => setTimeout(resolve, 4000)).then(copy);
  try {
    return (await Promise.race([network, slow.then((found) => found ?? network)]));
  } catch (error) {
    return (await copy()) ?? Response.error();
  }
}

async function kept(request) {
  const cache = await caches.open(cacheName);
  const copy = await cache.match(request);
  const update = fetch(request).then((response) => {
    if (response.ok) cache.put(request, response.clone());
    return response;
  });
  if (copy) {
    update.catch(() => {});
    return copy;
  }
  return update;
}

self.addEventListener("fetch", (event) => {
  const url = new URL(event.request.url);
  if (event.request.method !== "GET" || url.origin !== location.origin) return;
  event.respondWith(url.pathname.endsWith("/dictionary.sqlite3") ? kept(event.request) : fresh(event.request));
});
