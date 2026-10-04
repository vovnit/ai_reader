# AIReader for the web

[AIReader](../README.md) in a browser: an EPUB reader for reading in a
language you are still learning. Click a word and it is looked up in an
offline dictionary, and a model turns those entries into a short
explanation of what the word means *in that sentence*.

It is a progressive web app. Opened once, it works offline — the library,
the reader, the dictionaries; only the model and sync need the network — and
it can be installed from the browser's menu as an app of its own, which then
opens `.epub` files from the system's file manager (Chrome and Edge).

It does what the [Linux](../AIReaderQt/README.md) and [Kindle](../AIReaderKindle/README.md)
apps do, and syncs with them and the iOS app through the same WebDAV folder.

## Screens

- **Library** — the shelf, each group under its name. *Add…* picks `.epub`
  files, or drop them on the window. *Group…* puts a book in a group (books
  in one are searched together); *Remove* takes it off this device.
- **Reader** — click a word to look it up, blank space to turn the page; the
  arrow keys, Page Up/Down, Space, the wheel and a swipe turn pages too,
  Ctrl+F (⌘F) searches. *Menu* holds contents, the book's lookups, search,
  X-ray, a conversation about the page, display, close. Escape — or the
  browser's Back, or a phone's back gesture — closes the top screen.
- **Lookup** — the meaning in this sentence, the dictionary form, the word
  and the sentence spoken by the system's voices, and the way on to the
  dictionary's own entry, to what the book says about the word (X-ray), and
  to a conversation about it. On a wide window it opens beside the page with
  the word marked; on a phone it covers it.
- **Words** — every word looked up, the practice game, and export for Anki.
- **Settings** — the OpenAI-compatible endpoint and model, web search, and
  WebDAV sync; **Dictionaries** adds packs, word lists, Lingvo DSL, StarDict
  and XDXF.

## Running it

There is no build step: the folder is the app, plain HTML, CSS and
JavaScript modules, with no dependencies.

```sh
./build.sh serve
```

serves it at <http://localhost:8080>. Open
<http://localhost:8080/?aiEndpoint=mock://ai&aiModel=mock-medium> to answer
lookups from the mock instead of a paid model; `aiEndpoint`, `aiModel`,
`syncURL` and `syncUsername` in the address win over the saved settings for
that visit, without replacing them. A service worker and installing need a
secure origin: `localhost`, or HTTPS anywhere else.

`./build.sh dist` packs `dist/AIReader-web.zip` — the folder with the
bundled dictionary in it (here it is a link to the iOS app's copy) — to
unpack on any static web server: GitHub Pages, Netlify, a NAS, a folder
served by nginx.

## Hosting on Cloudflare

`wrangler.jsonc` describes a Worker with no script that serves the files
`./build.sh site` gathers into `dist/site` (the real dictionary in place of
the link). Static assets are served free; the dictionary, 17 MB, is under the
25 MiB limit per file. Cloudflare builds it from the repository on every push:

1. Commit and push `AIReaderWeb/` to GitHub.
2. In the Cloudflare dashboard: **Workers & Pages → Create → Workers →
   Import a repository**, connect GitHub, pick the repository.
3. Set:
   - **Project name:** `aireader` — it must match `name` in `wrangler.jsonc`.
   - **Root directory:** `AIReaderWeb`
   - **Build command:** `sh build.sh site`
   - **Deploy command:** leave `npx wrangler deploy`.
4. **Deploy.** The app is at `https://aireader.<your-subdomain>.workers.dev`;
   a domain of your own goes under the Worker's **Settings → Domains &
   Routes**. Each push to the production branch deploys again.

By hand instead: `sh build.sh site && npx wrangler deploy` from this folder,
after `npx wrangler login`.

The hosted app is a new address, so its library starts empty; sync brings it
over. A WebDAV server must allow the new origin (for rclone,
`--allow-origin https://aireader.<your-subdomain>.workers.dev`).

## Talking to services from a page

A page reaches a service only when the service answers requests from other
origins (CORS). Mistral, OpenAI and OpenRouter do. A local model server needs
telling: Ollama with `OLLAMA_ORIGINS`, LM Studio with its CORS switch. A
WebDAV server too: `rclone serve webdav --allow-origin https://your.host`,
or the CORS settings of Apache or nginx in front of it (allow `Authorization`,
`Depth` and `Content-Type`, and the methods `PROPFIND`, `MKCOL`, `PUT`,
`GET`). Monid, which brokers the web search, does not answer pages at
present, so `search_web` fails in the browser — the model is told and
answers without it.

## What is stored where

Everything stays in this browser, for this site:

| | |
| --- | --- |
| Books (their EPUBs and covers), groups, lookups and practice, deleted lookups, the books met in the sync folder, added dictionaries | IndexedDB, `aireader` (`src/Services/Schema.js`) |
| Endpoint, model, tokens, sync folder, user name and password, reading style | `localStorage` — not encrypted; a WebDAV account used only for AIReader is the sensible choice |
| The app and the bundled dictionary, for offline use | The service worker's cache |

The browser is asked to keep the library when space runs short once a book
is added. Another browser, or another site serving the app, has a library
of its own; sync brings them together.

## Layout

The same five layers as the other apps, each depending only on those
below: `Support` → `Domain` → `Services` → `Features` → `App`. Only
`Features/**/*View.js`, `Features/Common` and `App/main.js` touch the page;
everything below runs under Node, which is how it is checked.

| Folder | What lives there |
| --- | --- |
| `src/Support` | A tolerant XML scanner, ZIP reading and inflating through the browser's own `DecompressionStream`, text files in any encoding, a read-only SQLite reader, and IndexedDB with promises. |
| `src/Domain` | Books (package, navigation, chapter text, language, reading place, book key, file names), the AI prompts and the mock, the dictionary shape and formats, search, cards, reading style and word boundaries, and the sync document. |
| `src/Services` | The stores over IndexedDB, settings, the dictionaries, the EPUB loader, the corpus, the chat API and the tool-calling loop, web search, speech, WebDAV and sync. |
| `src/Features` | One folder per screen: a feature holding its state and actions, and a view that renders it. |
| `src/App` | `main.js`, which opens the library and the shelf, and `Env.js`, which makes the stores. |
| `tools` | `check.mjs` and the checks it runs; `MemoryDatabase.js` answers the IndexedDB calls from memory for them. |

There is no SQLite and no ZIP library. The dictionary packs are read by
`Support/SqliteFile.js`, which walks the file's b-trees page by page out of
the Blob it is kept as, so a pack of any size is searched without loading it.
Pages are laid out by the browser: the chapter flows into CSS columns one
page wide, and every text node remembers where it starts in the chapter's
text, so clicks, page starts and places are all chapter offsets, as in the
other apps.

## Checking it

```sh
./build.sh check ["../References/Reader/Reader/Le Petit Prince (Saint-Exupéry, Antoine de).epub"]
```

runs the Kindle check's cases against this code under Node 22.12 or later —
the HTML reduction, places, search, the mock, the prompts, cards, the sync
document and its merge, the stores, the dictionary converters, the bundled
dictionary through the SQLite reader, an EPUB through the loader — and
fails if the service worker's file list misses a file. With a WebDAV
server named:

```sh
rclone serve webdav /tmp/dav --addr 127.0.0.1:8765 --allow-origin http://localhost:8080 --user u --pass p
AIREADER_SYNC_URL=http://127.0.0.1:8765 AIREADER_SYNC_USER=u AIREADER_SYNC_PASSWORD=p ./build.sh check book.epub
```

three devices share the book through the folder and a sync round-trips. The
views are checked in a browser, against the mock.
