# AIReader

An EPUB reader for reading in a language you are still learning. Tap any word
and it is looked up in an offline dictionary, and a model turns those entries
into a short explanation of what the word means *in that sentence* — with the
dictionary form, how the form in front of you relates to it, and a translation
of the sentence as a whole.

SwiftUI, with Point-Free's [Composable Architecture][tca] and
[sqlite-data][sqlite-data].

[tca]: https://github.com/pointfreeco/swift-composable-architecture
[sqlite-data]: https://github.com/pointfreeco/sqlite-data

## What it does

**Library.** Add an `.epub` and it is unpacked into its own folder; the shelf
shows covers, newest first. Long-press a book to remove it, or to put it in a
group.

**Groups.** A group is for books that belong together, a series or a course:
the shelf lists each group under its name, and a search from any of its
books, the reader's or the model's, covers them all. The group's menu
dissolves it; the books stay.

**Reading.** The book is laid out as swipeable pages with its illustrations in
place. Where you stopped is remembered per book.

**Word lookup.** Tapping a word resolves it through the dictionary's form table
to its lemma, collects the articles, and sends them with the surrounding
sentence to the model. The answer gives the lemma, a note on the form in the
text, the meaning that fits *here*, and a translation of the sentence. If the
entries do not fit, the model can call the dictionary again for another form;
if nothing fits at all it guesses, says so, and reports its confidence. Answers
are cached, so the same word in the same sentence is paid for once.
*Dictionary entry* opens the article the answer was drawn from, as the
dictionary has it — every sense, and which dictionary it came from. It is
there only when the dictionary really has the lemma; a guess has no entry to
show. *X-ray* asks what the book, rather than the dictionary, makes of the
word (below). *Ask AI* opens a conversation that starts from the explanation —
for another example, a nuance, how the word differs from one like it.

**Search.** *Search*, in the menu, finds a word or phrase anywhere in the
book, or in every book of its group, case-insensitively, and lists the
sentences it occurs in, with the chapter. Tapping one turns the reader to
it — opening the other book, there, when it is in another book of the group.

**X-ray.** A name, a place, a word the author uses their own way: *X-ray*
gathers the passages where it has appeared and asks the model who or what it
is *in this book*, from those passages alone — not the dictionary, not what
the model knows of the book from elsewhere. Only the pages read so far are
searched, so nothing is given away; the other books of the group are searched
in full, since a series is read in order. The passages are listed under the
answer, each a tap from its place in the book. From the menu, X-ray asks for
a term; from a lookup, it takes the tapped word.

**The model reads the book too.** Whatever it is asked — a lookup, an X-ray,
a question in chat — the model has a `search_book` tool: the same search,
kept to the pages read so far. It uses it when a word looks like a name or an
invented term, or when a question is about what happened earlier.

**The model can search the web.** With a [Monid](https://monid.ai) token in
Settings, every prompt also offers `search_web`: for a name, a place, an
event, a piece of slang — the things neither the dictionary nor the book
explain. The model gets a few pages back, each with its address and a
snippet, in the book's language. Monid brokers many providers' search
endpoints behind one key; which endpoint answers, and the input it is sent,
are settings too, and the default is TinyFish's search. The tool is not
offered until a token is given, since most endpoints are paid per call. The
same settings as the Kindle app, kept in the shared suite so the Explain
extension searches the same way; the token lives in the keychain.

**Hearing it.** The word or the whole sentence can be spoken, in the language
the EPUB declares. On-device, free.

**The reader's menu.** A handle at the foot of the page opens:

- *Lookups* — every word met in this book, with the sentence it came from.
- *Search* and *X-ray* — above.
- *Ask about this page* — a conversation about the page in front of you. The
  page's text is sent once, with the first question, so follow-ups cost only the
  thread so far. A conversation opened from a lookup or an X-ray starts from
  that instead.
- *Display* — type size, face, line spacing and margins. Changes re-render and
  repaginate immediately, without re-reading the EPUB.

**Words.** From the library, the same list across every book — the vocabulary
actually encountered, rather than one somebody else chose.

**Practice.** Every word looked up is also a flash card, the way Anki keeps
one: the word on the front, the meaning it had on the back, and the sentence
it was met in with the word blanked out. *Practice*, on the Words and Lookups
screens, deals five of them — words down one column, meanings with their
sentences down the other, shuffled apart. Tap a word, then a meaning; a pair
that belongs together is put aside, one that does not is a miss. Each round
takes the cards that need it most — never practised first, then the most
missed, then the longest unseen — so a word that keeps getting confused keeps
coming back.

**Export.** Beside *Practice*, *Export* writes the words listed as the text
file Anki imports — the word with its lemma and the sentence on the front,
the meaning on the back, in a deck named `AIReader`, or `AIReader::<book>`
from one book's lookups — and hands it to the Files app to save.

**Sync.** *Settings → Sync* takes a WebDAV folder and an account. From then
on, reading positions, groups, looked-up words and how each has fared in
practice are shared through one file in that folder — with the [Kindle
app](AIReaderKindle/README.md), which reads and writes the same file. The app
syncs when it opens and when a book is closed, and on *Sync now*. Books are
matched by title and author, since neither the file nor the row is the same on
two devices; a position travels as the chapter, how far into it, and the words
at that point, so it is found again whatever the other device's rendering.
Where both devices changed one thing, the later change wins; a word deleted on
one device is deleted on the other rather than coming back.

The books themselves travel too, as EPUB files in the folder's `Books`: a
book added on one device is sent there, and one found there is fetched and
shelved. Removing a book removes it from that device only — the file stays
for the others, and is not fetched again. Web pages saved with the [browser
extension](BrowserExtension/README.md) land in the same folder and arrive
the same way.

## Layout

Logic stays out of the views: only `*View.swift` files are SwiftUI-specific, and
the rest would port to another UI layer unchanged.

| Folder | What lives there |
| --- | --- |
| `App` | The entry point: prepares the database and the root store. |
| `Database` | `appDatabase()` — connection, configuration, migrations. |
| `Support` | Inflate and deflate, an XML scanner, a ZIP reader, text files, image loading, and the keychain. |
| `Domain` | The material and pure logic over it: books (the package, the document with its chapters, the reading place, the book key, groups), the dictionary shape and formats, search (a phrase in a text, and the sentence around it), the AI prompts — explanation, chat, X-ray — the tools the model may call and the mock, cards (a flash card from a lookup, a round of the matching game, the Anki file) and the sync document with its merge. |
| `Services` | Anything reaching disk, network, database or keychain: the library, groups and cards in SQLite, the lookup cache, the corpus (the open book and its group, searched from any task), the chat API and the tool-calling loop, the dictionary packs, speech, and sync (the WebDAV client, the store that turns the database into a document and back, and the exchange of the books themselves). |
| `Features` | One folder per screen: a reducer and its views — library and group picker, reader, lookup with the dictionary entry, menu, search, X-ray, chat, words and practice, settings, dictionaries. |
| `App` | The entry point: prepares the database and the root store. |

There is no third-party EPUB or ZIP dependency: `EPUB/ZIPArchive.swift` reads
the archive directly, and `Support/Inflate.swift` wraps the system `Compression`
framework — which also decodes the dictionaries' zlib payloads.

## Dictionaries

A dictionary is a read-only `.sqlite3` pack from the [`DictionaryTool/`](DictionaryTool/README.md) pipeline:
`forms` maps every inflected form to a lemma and carries its grammar, `entries`
holds one zlib-compressed JSON article per lemma, and `metadata` states the
schema version and the language pair.

One pack ships inside the app — **French words with Russian definitions**. More
can be added under *Settings → Dictionaries*: pick a `.sqlite3` file and it is
copied in, its metadata read, and its languages shown. A pack whose
`schema_version` is not `2` is rejected rather than half-imported. Every enabled
pack is searched and the results are merged; when more than one answers, each
article says which pack it came from. The bundled pack can be disabled but not
deleted.

To build a pack for another language pair, from `DictionaryTool/`:

```bash
python3 process_dictionaries.py --target-language fr --definition-language en
```

It downloads its sources (Lexique4, Wiktionary extracts) next to itself, several
gigabytes that git ignores.

The language the *answers* are written in is the one named under **Explain in**
in Settings (Russian until changed), not the dictionary's.

## The model

*Settings* (the gear in the library toolbar) holds the endpoint, the token and
the model. Any OpenAI-compatible service works: the base URL is extended with
`/chat/completions` and `/models`, and **Load models** fills the picker from
whatever that endpoint offers. Changes save as they are made.

The endpoint and model are kept in `UserDefaults`; the token is a secret, so it
lives in the keychain instead — one left in preferences by an earlier build is
migrated on first read. Clearing the token is respected rather than treated as
unset, since a locally run endpoint usually wants no authorization at all.

### Parameter negotiation

Services disagree about request parameters. Newer OpenAI reasoning models reject
`max_tokens` in favour of `max_completion_tokens`, accept only their default
temperature, and refuse function tools unless `reasoning_effort` is `none`.
Rather than special-casing a provider, a rejected request is read for the
parameter it names, adjusted, and sent again; the adjustment is remembered for
that endpoint and model, so only the first lookup of a session pays for the
discovery. See `AI/RequestQuirks.swift`.

### The mock endpoint

Setting the endpoint to **`mock://ai`** answers lookups from `AI/MockAI.swift`
instead of the network. The mock reads the same conversation a real model would
and answers from the dictionary material in it, so a mocked run still exercises
the prompt, the tool-calling loop and the JSON parsing: it asks the dictionary
for another form when the first entries are empty, and marks an answer guessed
when nothing turns up. Use it for development instead of spending requests.

A test run can be pointed at it without touching the UI. Launch arguments land
in the argument domain, which outranks anything saved in Settings and lasts only
for that run:

```bash
xcrun simctl launch <device-udid> dev.nitochkin.AIReader \
  -aiEndpoint "mock://ai" -aiModel "mock-medium"
```

The sync folder can be given the same way (`-syncURL`, `-syncUsername`); the
password lives in the keychain and cannot.

(`xcrun simctl spawn <device> defaults write …` writes the *device-wide* domain,
which the app reads only until it saves settings of its own — after that its
container copy shadows it. Prefer the launch arguments above. The token is in
the keychain and is not reachable this way at all.)

## What is stored where

| | |
| --- | --- |
| Books and their groups, lookups and how each has fared in practice, deleted lookups (kept by name so a sync deletes them elsewhere too), dictionary packs | SQLite, via sqlite-data, in Application Support |
| Unpacked EPUBs, added dictionaries | Application Support, addressed by folder name so the container path can change |
| Endpoint, model, sync folder and user name | `UserDefaults` |
| API token, sync password | Keychain |
| Reading style | A JSON file, shared through `@Shared(.fileStorage)` |
| The sync document | `aireader-sync.json` in the WebDAV folder |
| Shared books | `Books/*.epub` in the WebDAV folder; which of them this device has met, in SQLite |

Schema changes are additive migrations, so an existing library survives an
update.

## Building

Xcode 27 or later, iOS 26 or later. Swift packages resolve on first build.

```bash
xcodebuild -project AIReader/AIReader.xcodeproj -scheme AIReader \
  -destination 'platform=iOS Simulator,name=iPhone 17 Pro' build
```

Two project settings are deliberate:

- `SWIFT_DEFAULT_ACTOR_ISOLATION = nonisolated`. Under the template's `MainActor`
  default every `@Reducer` fails to compile with "circular reference".
- sqlite-data is pinned to 1.12.0 or later. Earlier versions declare iOS 13 while
  their own dependency requires 16, which Xcode 27 rejects.
