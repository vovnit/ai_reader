# Working on AIReader

The app exists so someone can read a book in a language they are still learning
and understand what is in front of them. When a choice is open, pick the one
that serves comprehension and retention — not the one that adds a feature.

## Guidelines

**Simple language, simple code.** Prefer the plain solution, and change what
the task needs: a fix does not bring a refactor along, and nothing gets
configuration nobody asked for. Comments explain *why*, not what the line
already says. The code is maintained by one person across five apps, so every
extra idea costs five times.

**Minimal design, from system parts.** The reader is concentrating on a
foreign text; the interface should not compete with it. Use SwiftUI's own
components (`List`, `Form`, toolbars, sheets, SF Symbols) with system colors
and fonts. Avoid custom colors, gradients, shadows, animations and decorative
icons; the reading style is the only place the app picks a typeface. Anything
that would need explaining is too much. The Kindle, Qt, web and
browser-extension interfaces follow the same rule with their own toolkits'
plain widgets — on the web, plain HTML elements and the system's colors.

**Keep logic away from the UI.** Views render state and send actions, nothing
else. Parsing, lookup, pagination, sync and networking live below the view
layer, because the same logic runs on iOS, Kindle, Linux and the web, and
anything written into a view has to be written again for each.

**Keep files small.** One idea per file, and under about 250 lines. When a
file starts covering two ideas, split it: small named pieces are what make
this codebase navigable without an index.

**Use TCA and sqlite-data** — Point-Free's Composable Architecture for state
and effects, sqlite-data for persistence — and follow their idioms rather than
inventing a parallel mechanism, so that their documentation stays the
documentation for this code.

**Test against the mock, not the wallet.** The endpoint `mock://ai` answers
from `Domain/AI/MockAI.swift` (`MockAI.cpp` on Kindle and Qt, `MockAI.js` on
the web). Use it for
every routine run. Real API calls cost the owner money: when one is genuinely
needed, keep it minimal and say what it cost.

**No secrets in the tree.** Tokens and passwords come from the person using
the app — Settings, stored in the keychain (iOS) or the settings file (Kindle,
Qt, extension storage, the web app's browser storage) — or, for development, from `MISTRAL_API_KEY` in the
environment. A key once written into source ends up in every build and package
made from it, so it has to be revoked, not just deleted.

**`References/` is dirty examples.** Read it for orientation; do not copy from
it. Git ignores it.

## Before changing something, find its twins

Much of this project is written more than once, deliberately, so the apps
agree. A change to one copy that misses the others builds and passes checks,
then breaks sync or the file format between devices. Before editing any of
these, open every copy:

- **A feature's logic.** The Kindle app's `*Feature.cpp` is also the Qt app's
  (compiled unchanged, globbed). The iOS app has its own reducer, the web app
  its own `AIReaderWeb/src/Features/*/*Feature.js`. Change behaviour in all
  three; the Qt view only renders.
- **What the model is asked, and how a chapter becomes text.** The prompts,
  tool descriptions and the mock are word for word alike in
  `AIReader/AIReader/Domain/AI`, `AIReaderKindle/src/Domain/AI` and
  `AIReaderWeb/src/Domain/AI`. `HtmlText` decides which chapters are kept, so
  chapter numbers agree between devices: `HtmlText.cpp`, `HtmlText.js`, and
  the iOS loader.
- **The database schema.** iOS migrations in `App/AppDatabase.swift`, Kindle
  and Qt in `AIReaderKindle/src/Services/Migrations.cpp`. Migrations are
  additive: an existing library must survive an update. The Kindle check
  rewinds a library to version 1 and migrates it forward, so a new migration
  must also teach that check what to drop. The web app keeps the same records
  in IndexedDB (`AIReaderWeb/src/Services/Schema.js`), additive the same way.
- **The sync document.** Reading, writing and merging exist only in
  `Core/Sources/AIReaderCore/Domain/Sync/SyncDocument.cpp`, compiled by both
  apps. `Domain/Sync/SyncDocument.swift` is its Swift face and
  `SyncDocument+Core.swift` the only place that converts; keep C++ types out
  of other Swift files. The web app cannot compile C++, so
  `AIReaderWeb/src/Domain/Sync/SyncDocument.js` is a port: change it with the
  C++, and `AIReaderWeb/build.sh check` repeats the C++ check's cases.
- **The book key** (title and author, normalized), which matches a book across
  devices: `Domain/Books/BookKey.swift`,
  `AIReaderKindle/src/Domain/Books/BookKey.cpp` and
  `AIReaderWeb/src/Domain/Books/BookKey.js`. It needs Unicode lowercasing,
  which `Core/` cannot do without a library.
- **Shared books.** The file-name rule for the sync folder's `Books`:
  `RemoteBookName.swift`, `RemoteBookName.cpp`, `RemoteBookName.js` (web) and
  `BrowserExtension/lib/names.js`. The exchange itself: `LibrarySync.swift`,
  `LibrarySync.cpp` and `LibrarySync.js` (web).
- **WebDAV.** `Services/Sync/WebDAV.swift`, `AIReaderKindle/src/Services/WebDav.cpp`,
  `AIReaderWeb/src/Services/WebDav.js` and `BrowserExtension/lib/webdav.js` must agree on folder creation (a missing
  parent makes `MKCOL` answer 409) and on escaping names.
- **User-visible descriptions of sync**, in each app's Settings screen and
  README.
- **The table of contents.** `EpubNavigation` (Kindle, web) and
  `EPUBNavigation.swift` read it; each app's loader points it into the kept
  chapters the same way — `EpubLoader.cpp`, `Contents.js`, and on iOS
  `BookDocumentLoader.swift` with `ChapterAnchors.swift`, which finds the
  anchors the HTML import would lose.
- **How a PDF becomes a book.** The rules that turn a PDF's lines into
  paragraphs and chapters, and the EPUB written from them, are alike in
  `PdfParagraphs`, `PdfLayout` and `EpubBuilder` (Kindle, `.cpp`; web,
  `.js`) and `PDFParagraphs`, `PDFLayout` and `EPUBBuilder` (iOS, `.swift`).
  The PDF itself is read by `Support/Pdf*.cpp` and their `.js` ports, and on
  iOS by PDFKit (`PDFImporter.swift`). The web app's `Support/ZipWriter.js`
  is the browser extension's `lib/zip.js`. The two checks put together the
  same sample PDF; given the same real one, the Kindle and web apps should
  write the same EPUB.
- **A book's offline glossary.** Its prompt is `GlossaryPrompt` in each
  app's `Domain/AI` and `DictionaryTool/glossary_prompt.py`, word for word,
  with the mock's answer to it; the words come from `BookWords`, by each app's
  own word boundaries (Pango splits at an apostrophe, the others do not). A
  book's glossary is the dictionary named "<title> glossary" everywhere.
- **The bundled dictionary's credits** — its sources and their license, which
  the license requires be shown — on each app's Dictionaries screen and in the
  top-level README. A pack built from other sources changes all of them.

## Layout

The top level has one folder per thing that is built:

| Folder | What it is |
| --- | --- |
| `AIReader/` | The iOS app, and `LookupExtension/`, the Explain action extension. |
| `AIReaderKindle/` | The Kindle app: C++17, GTK+ 2, Meson. |
| `AIReaderQt/` | The Linux desktop app: Qt 6 Widgets views over the Kindle app's code. |
| `AIReaderWeb/` | The web app, a PWA: plain HTML, CSS and JavaScript modules, no build step. |
| `Core/` | C++ both the iOS and Kindle apps compile: the sync document and JSON. |
| `BrowserExtension/` | Chrome and Firefox: saves web pages as EPUBs into the sync folder. |
| `DictionaryTool/` | Python: builds the bundled `dictionary.sqlite3` and other packs. |
| `ScanTool/` | Python: makes an EPUB of a scanned book from its Mistral OCR export. |

Build output, wherever it lands, is ignored by git, and so are the dictionary
tool's gigabytes of downloaded source data.

**iOS.** Source sits in five layers, each depending only on the ones below:
`Support` (generic utilities) → `Domain` (books, words, dictionaries, and pure
logic over them) → `Services` (anything reaching disk, network, database or
keychain) → `Features` (a reducer and its views) → `App` (composition). A file
that needs something from a layer above it is in the wrong layer. Features are
reducers; everything touching the outside world is a `@DependencyClient`, so it
can be swapped in tests and previews. SwiftUI appears in `*View.swift` and at
the edges only: the two entry points, `Support/PlatformImage.swift` and
`AnkiCardsDocument` (a `FileDocument`). `DisplaySettingsFeature.swift` still
holds its view; split it out when the file is next touched.

**Persistence** is split by kind: SQLite for records, `UserDefaults` for plain
settings, keychain for the token and the sync password,
`@Shared(.fileStorage)` for reading style.

**The Explain extension** is a second composition root that compiles the whole
`AIReader/` folder except the app entry point, the asset catalog and the
bundled dictionary (read from the host app's bundle instead). What both
processes need — the database, added dictionaries, AI settings, the token —
lives in the app group named in `Support/AppGroup.swift`; the app moves older
files there on launch. Books and reading style stay in the app's own container.

**Qt** compiles the Kindle app's `Support`, `Domain`, `Services` and
`*Feature.cpp` unchanged and adds one `*View` per screen owning its feature.
It relies on Qt running a GLib event loop on Linux, which
`Support/Async.hpp` posts results through.

**Web.** The same five layers under `AIReaderWeb/src`; only `*View.js`,
`Features/Common` and `App/main.js` touch the page, so everything below runs
under Node and is checked there. Records live in IndexedDB, settings in
`localStorage`. Dictionary packs are read by `Support/SqliteFile.js`, a
read-only reader of the SQLite file format, and pages are laid out by the
browser in CSS columns, every text node knowing its chapter offset.
`dictionary.sqlite3` there is a link to the iOS app's copy.

**AI endpoints.** The apps talk to any OpenAI-compatible endpoint. Do not
special-case a provider; if a service rejects a parameter, negotiate from the
error it returns (`Domain/AI/RequestQuirks.swift`).

## Verifying a change

Check the piece you changed the cheapest conclusive way, then say plainly what
was verified and what was not. Command-line checks are usually faster and more
conclusive than driving a UI, and cost nothing.

| What changed | How to check it |
| --- | --- |
| iOS code | Build (below), then run against the mock in the simulator. |
| Pure Swift logic (prompts, mock replies, formatting, ZIP, names) | Compile the files with a small `main.swift` using `swiftc` and run it. |
| Kindle, Qt shared code, or `Core/` | `AIReaderKindle/build.sh check` (runs on a Mac). |
| Anything that talks to a WebDAV server | The same check with `AIREADER_SYNC_URL` (and `AIREADER_SYNC_USER`, `AIREADER_SYNC_PASSWORD`) set and a book given, against a local server: `rclone serve webdav <dir> --addr 127.0.0.1:8765`. |
| An EPUB, from any source | `AIReaderKindle/build.sh check book.epub page.png` also loads it and renders a page. |
| PDF import | `AIReaderKindle/build.sh check book.pdf page.png` and `AIReaderWeb/build.sh check book.pdf` make the EPUB and load it; the two should agree. |
| Qt views | `AIReaderQt/build.sh` (Docker); `AIReaderQt/build.sh script tools/smoke.txt <book.epub>` (written for the Le Petit Prince in `References/`) walks every screen against the mock and saves snapshots. |
| The Qt AppImage | `AIReaderQt/build.sh appimage`; test it in a clean container of another distribution, which needs `shared-mime-info` and a desktop's X/GL libraries (see `AIReaderQt/README.md`). |
| The browser extension | Load it unpacked in Chrome, or in Playwright's Chromium with `--load-extension`, against a local WebDAV server and page. `lib/zip.js`, `lib/epub.js` and `lib/names.js` also run under Node; the rest need a browser. |
| Web logic (`AIReaderWeb/src` below the views) | `AIReaderWeb/build.sh check [book.epub]` (Node 22.12+); with `AIREADER_SYNC_URL` set, against a WebDAV server too. |
| Web views | `AIReaderWeb/build.sh serve`, then `http://localhost:8080/?aiEndpoint=mock://ai&aiModel=mock-medium` in a browser. Sync from a page needs the server to allow its origin: `rclone serve webdav <dir> --addr 127.0.0.1:8765 --allow-origin http://localhost:8080`. |
| `DictionaryTool/` | `python3 -m unittest discover -s tests -t .` from that folder. |
| `ScanTool/` | The same unit tests from that folder; then convert an export and load the EPUB with `AIReaderKindle/build.sh check book.epub page.png`. |

Build the iOS app:

```bash
xcodebuild -project AIReader/AIReader.xcodeproj -scheme AIReader \
  -destination 'platform=iOS Simulator,name=iPhone 17 Pro' \
  -skipMacroValidation build
```

Run it against the mock. Launch arguments override saved settings for that
launch only; `-syncURL` and `-syncUsername` work the same way, but the sync
password lives in the keychain, so test sync against a server without one:

```bash
xcrun simctl launch <device-udid> dev.nitochkin.AIReader \
  -aiEndpoint "mock://ai" -aiModel "mock-medium"
```

The Explain extension is its own process, so launch arguments do not reach it.
To run it against the mock, put the endpoint in the shared suite instead:

```bash
xcrun simctl spawn <device-udid> defaults write \
  "$(xcrun simctl get_app_container <device-udid> dev.nitochkin.AIReader groups | awk '/group.dev/ {print $2}')/Library/Preferences/group.dev.nitochkin.AIReader.plist" \
  aiEndpoint "mock://ai"
```

Then select text in Safari or any app and choose Explain in the share sheet.
Safari runs `LookupExtension/Action.js` only when the *page* is shared (with
text selected); sharing from the selection's own menu delivers the bare text.

## Project gotchas

- `SWIFT_DEFAULT_ACTOR_ISOLATION = nonisolated` is required. Under the
  template's `MainActor` default, every `@Reducer` fails with "circular
  reference" and the real cause is not reported.
- sqlite-data must be 1.12.0 or later; earlier versions declare iOS 13 while
  their own dependency requires 16, which Xcode 27 rejects.
- `xcodebuild` needs `-skipMacroValidation`, since the macro packages are not
  approved outside the Xcode UI.
- The extension target must carry the same `SWIFT_DEFAULT_ACTOR_ISOLATION`
  and package products as the app; it is embedded with an `ios` platform
  filter, so Mac and visionOS builds leave it out.
- Both targets set `SWIFT_OBJC_INTEROP_MODE = objcxx`. Every Swift module
  that imports a C++ one, even through another Swift module, needs it, so the
  interop cannot be hidden inside a package. C++ `static constexpr` members do
  not reach Swift; a `static const` defined in the `.cpp` does. Do not name a
  C++ module `Cxx` — Swift already has one.
- `.onChange(of:)` attaches to the single reducer expression before it, not to
  the whole `body`. Put `BindingReducer()` and `Reduce` inside
  `CombineReducers` first, or binding edits are invisible to it — this silently
  broke saving settings once already.
- The Kindle app's `Async::run` drops a completion once its screen's guard has
  expired. Anything that must be recorded whatever the screen does — what a
  sync sent or fetched — runs under a guard that never expires, or the next
  run repeats the work.
- The web app's `sw.js` lists every file it keeps for offline use; a new file
  missing from the list is missing offline, and `AIReaderWeb/build.sh check`
  fails on it.
- A web page reaches only services that allow its origin (CORS). Mistral,
  OpenAI and OpenRouter do; Nextcloud's WebDAV and Monid do not. Hosted on
  Cloudflare, `AIReaderWeb/relay.js` relays the hosts named in the Worker's
  `RELAY_HOSTS` variable; served anywhere else, a server must allow the
  page's origin itself.
- Chrome refuses to inject a script containing the characters U+FFFE or U+FFFF,
  reporting that it "isn't UTF-8 encoded". Keep `BrowserExtension/*.js` ASCII,
  with `\u` escapes in regular expressions.
