# AIReader for Kindle

The Kindle version of [AIReader](../README.md): an EPUB reader for reading in
a language you are still learning. Tap a word and it is looked up in an
offline dictionary, and a model turns those entries into a short explanation
of what the word means *in that sentence*.

Native C++17 with GTK+ 2 — what a jailbroken Kindle runs — built with Meson
against [koxtoolchain](https://github.com/koreader/koxtoolchain) and the
[Kindle SDK](https://github.com/KindleModding/kindle-sdk), and shipped as a
[KPM](reference/kindle-dev/kpm/_index.md) package. The same code builds and
runs on a desktop for development.

## What it does

The same things as the iOS app, in the same shape.

**Library.** *Add* opens a file picker — the folders of the Kindle's USB
partition, one tall row per entry — and copies the `.epub` you choose into
`/mnt/us/aireader/books`. Files copied there (or into the Kindle's own
`documents` folder) over USB appear after *Refresh*. Where you stopped is
remembered per book. `✕` removes a book and its file.

**Groups.** *Group*, on a book's row, puts it in a group — one of the groups
there are, or a new one typed in — and the shelf lists each group under its
name. A group is for books that belong together, a series or a course: a
search from any of them, the reader's or the model's, covers them all. `✕` on
the group's line dissolves it; the books stay.

**Reading.** The book is laid out as pages of justified, indented text with
bold headings, italics and superscripts kept, and its illustrations in place,
shrunk to fit the page. Footnote marks are left out: there is no following a
link here, and a number glued to a word would get in the way of looking the
word up. Tap blank space, or `‹` `›`, to turn the page; the footer says where
you are and opens the menu. On the MediaTek Kindles — PaperWhite 5 and
everything since — the new page slides in the way it does in the Kindle's own
reader: the display driver plays that animation itself when asked to, which
is what KOReader does, and `src/Services/EinkDisplay.cpp` asks it the same
way, putting the page straight on the panel before X draws it. Older Kindles
and the desktop just show the next page. *Display* switches the slide off.

**Word lookup.** Tapping a word resolves it through the dictionary's form table
to its lemma, collects the articles, and sends them with the surrounding
sentence to the model. The answer gives the lemma, a note on the form in the
text, and the meaning that fits *here*. If the entries do not fit, the model
calls the dictionary again for another form; if nothing fits it guesses, says
so, and reports its confidence. Answers are cached, so the same word in the
same sentence is paid for once. *Dictionary entry* opens the article the
answer was drawn from, as the dictionary has it — every sense, and which
dictionary it came from. It is there only when the dictionary really has the
lemma; a guess has no entry to show. *X-ray* asks what the book, rather than
the dictionary, makes of the word (below). *Ask AI* opens a conversation that
starts from the explanation — for another example, a nuance, how the word
differs from one like it.

**Search.** *Search*, in the menu, finds a word or phrase anywhere in the
book, or in every book of its group, case-insensitively, and lists the
sentences it occurs in, with the book and chapter. Tapping one turns the
reader to it — opening the other book, there, when it is in another book of
the group.

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
invented term, or when a question is about what happened earlier. The answer
then says what the book says.
In a conversation it also has the dictionary, `lookup_dictionary`, the same
tool a lookup gives it: asked what a word means or how two words differ, it
answers from the article rather than from memory.

**The model can search the web.** With a [Monid](https://monid.ai) token in
Settings, every prompt — a lookup, an X-ray, a conversation — also offers
`search_web`: for a name, a place, an event, a title, a piece of slang, the
things neither the dictionary nor the book explain. The model gets a few
pages back, each with its address and a snippet, in the book's language,
and answers from those. Monid brokers many providers' search endpoints
behind one key (`src/Services/WebSearch.cpp` runs the endpoint and polls
when it works in the background). Which endpoint answers, and the input it
is sent, are settings; the default is TinyFish's search, which Monid lists
at no charge, and `monid discover -q "web search"` lists the others with
their prices — Exa, Octen, Firecrawl and the rest are paid per call, from a
hundredth of a cent to a cent. The tool is not offered until a token is
given, each run's cost, when there is one, goes to the log, and under the
mock endpoint the search is answered by `MockAI` without the network.

**The menu.** *Contents* — the book's own table of contents, from its
navigation document (EPUB 3) or NCX (EPUB 2), nested entries indented and
the one being read marked; a tap turns to it, to the very anchor when the
entry points inside a file. A book without one lists its chapters by their
first heading. *Lookups* — every word met in this book, with its sentence.
*Search* and *X-ray* — above. *Ask about this page* — a conversation about
the page on screen; the page's text is sent once, with the first question. A
conversation opened from a lookup or an X-ray starts from that instead, so
follow-up questions need no retyping. *Display* — type size, face, line
spacing and margins, and whether page turns are animated; the page
re-paginates as they change. *Close book.*

**Words.** From the library, the same list across every book.

**Practice.** Every word looked up is also a flash card, the way Anki keeps
one: the word on the front, the meaning it had on the back, and the sentence
it was met in with the word blanked out. *Practice*, on the Words and Lookups
screens, deals five of them — words down one column, meanings with their
sentences down the other, shuffled apart. Tap a word, then a meaning; a pair
that belongs together is put aside, one that does not is a miss. Each round
takes the cards that need it most — never practised first, then the most
missed, then the longest unseen — so a word that keeps getting confused keeps
coming back.

**Export.** Beside *Practice*, *Export* writes the words listed to
`anki-cards.txt` in the app's folder, visible over USB, as the text file
Anki imports: the word with its lemma and the sentence it was met in on the
front, the meaning it had there on the back, in a deck named `AIReader` — or
`AIReader::<book>` when exported from one book's lookups.

**Typing.** Chat, Search, X-ray, group names and Settings carry their own on-screen keyboard — Latin with
the accents French needs, Cyrillic, and digits with punctuation. The Kindle's
own keyboard reaches a GTK window only as X key events, which carry Latin and
nothing else, so it is not used.

**Sync.** *Settings* takes a WebDAV folder and an account. From then on,
reading positions, groups, looked-up words and how each has fared in practice
are shared through one file in that folder — with the [iOS app](../README.md),
which reads and writes the same file. The app syncs quietly when it opens and
when a book is closed, and *Sync now* in Settings says what came and went.
Books are matched by title and author, since neither the file nor the row is
the same on two devices; a position travels as the chapter, how far into it,
and the words at that point, so it is found again whatever the other device's
rendering — a page here, a different page size there. Where both devices
changed one thing, the later change wins; a word deleted on one device is
deleted on the other rather than coming back.

The books themselves travel too, as EPUB files in the folder's `Books`: a
book added on one device is sent there, and one found there is fetched and
shelved in `/mnt/us/aireader/books`. Removing a book removes it from that device only — the file stays
for the others, and is not fetched again. Web pages saved with the [browser
extension](../BrowserExtension/README.md) land in the same folder and arrive
the same way.

**Settings.** Endpoint, token and model of any OpenAI-compatible service, with
*Load models* to fill the picker. A second token is kept for OpenAI and used
whenever the endpoint is `api.openai.com`, so switching services does not mean
retyping. A Monid token, with the search provider, endpoint and input,
lets the model search the web (above). *Dictionaries* lists the packs and
lets each be switched off. *Add*
picks a dictionary anywhere on the device through the same file picker and
copies it into `/mnt/us/aireader/dictionaries` (a StarDict set travels
together, whichever of its files was picked); *Scan folder* registers whatever
has been copied there by hand. Packs in this app's format (`.sqlite3`) are
taken as they are; word lists (`.tsv`, `.csv`), Lingvo DSL (`.dsl`, plain or
`.dz`), StarDict (`.ifo` with its `.idx` and `.dict`/`.dict.dz`) and XDXF are
converted into one, which for a large dictionary takes a while.

**Looks.** GTK+ 2 draws its controls the way a desktop of the nineties did.
Every control here is drawn by the app instead (`src/Features/Common/Theme.cpp`):
black on white, thin rounded outlines, a black fill while a button is pressed,
a tick in a square for a check box, a hairline for a separator, a thin bar
for a scrollbar — the shapes the Kindle's own screens use, and what e-ink
shows well.

Not carried over from iOS: hearing a word spoken — a Kindle has no speech.
The language a book is written in is still read off its prose (metadata is
often wrong); here it feeds the lookup records and Pango's line breaking.

The explanations and answers, like the iOS app's, are written in the language
named under *Explain in* in Settings (`language` in `settings.ini`), Russian
until changed.

## Layout

Five layers, each depending only on the ones below it. Only `*View.cpp`
files and `App/` know GTK.

| Folder | What lives there |
| --- | --- |
| `../Core` | Shared with the iOS app, which compiles it through Swift's C++ interop: JSON, and the sync document with its merge. Standard library only. Included by path from there: `"Support/Json.hpp"`. |
| `src/Support` | An XML scanner, a ZIP reader over zlib, files, and running work off the main loop. |
| `src/Domain` | Books (the package document, the navigation document or NCX, HTML to text, language detection, the reading place any device can find again), dictionaries (the lookup and its prompt summary, normalizing), the dictionary file formats, search (a phrase in a text, and the sentence around it), AI (messages, the prompts — explanation, chat, X-ray — the tools the model may call, the dictionary, the book and the web, the mock, parameter negotiation), cards (a flash card from a lookup, a round of the matching game), the key a book goes by across devices, and reading (pagination, illustrations and word/sentence resolution, on Pango and gdk-pixbuf). |
| `src/Services` | SQLite (library and groups, lookup cache and the deleted lookups it remembers, card practice, dictionary packs, the packs themselves), the settings file, the e-ink panel (the page-turn slide of the MediaTek Kindles), one HTTP request on libcurl and, on it, the chat API, the web search through Monid and the WebDAV client, the corpus (the open book and its group, searched from any thread), the tool-calling loop that answers the model's dictionary, book search and web search calls, and sync (the store that turns the database into a document and back, and the round trip to the server). |
| `src/Features` | One folder per screen: a `*Feature` holding state and actions, and a `*View` that renders it. `Common/` holds the navigation stack, the shared widgets, the theme that draws them, the dialogs, the keyboard and the sync runner the library and the settings share. `FilePicker/` is the screen the library and the dictionaries use to take a file from anywhere on the device. Screens opened from the reader — lookups, search, X-ray, the menu — carry a `ReaderLink`: the books to search and a way back to a place in them. |
| `src/App` | `main`, the one-window navigation stack, and the smoke script. |
| `package` | The KPM package: manifest, hooks, scriptlet, and the build scripts (one for an installed toolchain, one that does it all in Docker). |
| `tools/check.cpp` | The command-line check of everything below the views. |
| `build.sh` | The one entry point: desktop build, `check`, `run` against the mock, `kindle` package. |

Features are plain classes with actions as methods and an `onChange` callback;
views build widgets, send actions, and re-render on change. Anything slow —
opening a book, a lookup, a chat turn, the model list — runs on a thread
through `Support/Async.hpp` and lands back on the main loop, dropped if its
screen is gone.

## What is stored where

Everything is under `$AIREADER_HOME`: `/mnt/us/aireader` on the Kindle (set by
`launch.sh`, visible over USB), `~/.aireader` on a desktop.

| | |
| --- | --- |
| `books/` | Where `.epub` files go. Read in place; nothing is unpacked. |
| `dictionaries/` | Added packs and word lists. |
| `library.sqlite3` | Books, positions and groups, lookups and how each has fared in practice, the lookups deleted (kept by name so a sync deletes them elsewhere too), the dictionary list. |
| `settings.ini` | Endpoint, model, `token`, `openai_token`, the answer `language`, the web search (`[web]`: `monid_token`, `provider`, `endpoint`, `input`), the sync folder (`[sync]`: `url`, `user`, `password`), reading style and `turn_animation`. There is no keychain on a Kindle, so the tokens and the password sit here too. Editable from a computer; the token starts as the development one the iOS app also uses. |
| `anki-cards.txt` | The last export for Anki. |

The bundled French → Russian dictionary is the iOS app's
`AIReader/AIReader/Resources/dictionary.sqlite3`, copied into the package by
`package/build.sh` and found through `$AIREADER_DATA_DIR`.

## Building for a desktop

GTK+ 2, Pango, SQLite, zlib and libcurl through pkg-config; Meson and a C++17
compiler. On macOS: `brew install gtk+ meson` (curl, sqlite and zlib come with
it or the system).

```bash
./build.sh
```

That is `meson setup build` the first time, then `meson compile -C build`.

Run against the mock, which answers from `src/Domain/AI/MockAI.cpp` and costs
nothing. The arguments override the saved settings for that run only:

```bash
./build.sh run
```

which is `AIREADER_DATA_DIR=../AIReader/AIReader/Resources ./build/aireader
--endpoint mock://ai --model mock-medium`; further arguments are passed on.

The pure logic — prompt building, mock replies, dictionary formatting,
parameter negotiation, EPUB reading and its table of contents, pagination,
the database, the sync document and its merge — runs from the command line, no display needed. Give
it an EPUB to read that too, and a PNG path to draw its first page:

```bash
./build.sh check book.epub page.png
```

With `AIREADER_SYNC_URL` (and `AIREADER_SYNC_USER`, `AIREADER_SYNC_PASSWORD`)
naming a WebDAV folder, the check also makes a round trip through it.

The interface can be walked by a script where there is nothing to tap with:
`AIREADER_SCRIPT=walk.txt` runs one command per line — `tap x y`, `press
label` (taps the first button with a label reading that, on any of its lines,
wherever it is), `type
text`, `wait ms`, `dump` (prints the visible widgets and their positions),
`snap file.png` (on X11), `quit`. To see the screens at a Kindle's size, build the
Linux variant in Docker (`package/docker/Dockerfile.linux-desktop`) and run it
under Xvfb with `AIREADER_WINDOW_SIZE=1236x1648`; every size in the interface
scales with the window width, so that is what the device shows.

## Building for a Kindle

With Docker (OrbStack, Docker Desktop) and Python 3.12+, one command does it:

```bash
./build.sh kindle kindlehf
```

It builds a Linux container, downloads the pre-built koxtoolchain, generates
the Kindle SDK on top (which loop-mounts a firmware image, so the container is
privileged), cross-compiles, and packs
`dist/aireader_0.1.0_kindlehf.kpkg`. The toolchain and SDK stay in Docker
volumes, so the second run takes seconds. `kindlehf` is any Kindle on
firmware 5.16.3 or later; pass `kindlepw2` for older firmware.

With koxtoolchain and the SDK installed locally (see
[reference/kindle-dev/gtk-tutorial](reference/kindle-dev/gtk-tutorial/prerequisites.md)),
`package/build.sh <meson-crosscompile.txt> kindlehf` does the compile-and-
assemble part, and KPM's `kpm-helper.py package pack dist/aireader dist` the
packing.

Install the `.kpkg` with KPM on the device. That drops an *AIReader* entry into
the Kindle library; opening it runs `launch.sh`.

Two things about the target are worth knowing. The Kindle's libraries are
older than the SDK's `.pc` files claim — GLib 2.29, GTK+ 2.20, Pango 1.26,
gdk-pixbuf 2.20 — so the code sticks to that API (`g_thread_create` rather
than `g_thread_new`, and so on), which still exists, deprecated, on a desktop.
Pango 1.26 also applies a scale attribute as a point size, whatever the base
size was set in, so every text size — headings included — is given
absolutely, in pixels.
And koxtoolchain's GCC is newer than the Kindle's, so the C++ runtime is linked
statically when cross-compiling.

On the device the app is one full-screen window titled the way the Kindle's
window manager expects (`L:A_N:application_ID:org.aireader.kindle_PC:N`).
Interface sizes follow the screen width (`AIREADER_UI_SCALE` and `AIREADER_UI_FONT` in
`launch.sh`'s environment override that). Output goes to
`/mnt/us/aireader/aireader.log`.

## What was verified

On macOS, against the mock and the bundled dictionary: `aireader-check`
passes every check, including the tool-calling loop, the book search and its
excerpts, the corpus kept to the reader's position, the mock searching the
book from a chat and from an X-ray, the web search's reading of a provider's
answer and the mock searching the web from a chat, groups, the DSL, StarDict and
XDXF conversions, adding a dictionary from another folder by one of its
files, the cards and the matching round, and a library from before the
practice table and the groups migrating forward; the scripted walk opens a
real, illustrated EPUB, turns pages, opens the contents and jumps to a
chapter from it, looks words up and opens their
dictionary entries, lists lookups, chats, changes the display,
adds dictionaries and lists models, adds a book and a word list through the
file picker, plays the matching game through a pair, a miss, a whole
round and the next, puts two books in a group, searches them and jumps to a
hit in each, X-rays a name from the menu and a word from a lookup, and
carries an explanation and an X-ray into a conversation, and asks a
conversation to search the web; every screen was
looked at as a window capture after the controls were redrawn.

Sync was walked end to end against a small WebDAV server on the same machine:
this app read four pages of a book and synced; the iOS app, in the simulator,
picked the book up at that page, looked words up, read on, grouped the book
and practised, and synced; this app then received the words, the group and
the practice, and opened the book on the page the iOS app had reached.

The `kindlehf` package cross-compiles in the container: an ARM hard-float
binary against the firmware's own libraries, glibc symbols within the Kindle's
2.20, no dynamic libstdc++. What has not been done is running it on a device —
the window manager, touch, the keyboard, e-ink refresh, the page-turn slide
and fonts are untested there. The web search has not been run against
Monid itself: the request and the polling follow Monid's API documentation
and its CLI, and the reading of the answer was checked on TinyFish's
documented shape and on two others, but no run has been made, so the
default input is unconfirmed — `monid inspect -p tinyfish -e /search`
shows what the endpoint takes today.
