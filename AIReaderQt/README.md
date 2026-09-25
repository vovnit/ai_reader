# AIReader for Linux

The desktop version of [AIReader](../README.md), in Qt 6 Widgets: an EPUB
reader for reading in a language you are still learning. Click a word and it
is looked up in an offline dictionary, and a model turns those entries into a
short explanation of what the word means *in that sentence*.

It does everything the [Kindle app](../AIReaderKindle/README.md) does, and is
the same program under its screens: the Kindle app's support, domain,
services and feature state are compiled in unchanged, along with what
[`../Core`](../Core) shares with iOS. Only the views are new. Pages are laid
out by the Kindle's Pango paginator and drawn through cairo at the screen's
density, so a page breaks the same way on both.

## Screens

- **Library** — the shelf, each group under its name. *Add…* copies an
  `.epub` into `~/.aireader/books`; files copied there appear on *Refresh*.
  *Group* puts a book in a group (books in one are searched together); `✕`
  removes a book or dissolves a group.
- **Reader** — click a word to look it up, blank space to turn the page; the
  arrow keys, Page Up/Down, Space and the wheel turn pages too. The footer
  opens the menu: contents, the book's lookups, search (also Ctrl+F), X-ray,
  a conversation about the page, display, close. Escape goes back from any
  screen.
- **Lookup** — the meaning in this sentence, the dictionary form, and the way
  on to the dictionary's own entry, to what the book says about the word
  (X-ray), and to a conversation about it.
- **Words** — every word looked up, the practice game, and export for Anki.
- **Settings** — the OpenAI-compatible endpoint and model, web search, and
  WebDAV sync with the iOS and Kindle apps; **Dictionaries** adds packs, word
  lists, Lingvo DSL, StarDict and XDXF.

Everything lives in `~/.aireader` (or `$AIREADER_HOME`), the same place as
the Kindle app's desktop build, so the two share a library.

## Building

On Linux with Qt 6, Pango, gdk-pixbuf, SQLite, zlib and libcurl:

```sh
cmake -B build -G Ninja && cmake --build build
AIREADER_DATA_DIR=../AIReader/AIReader/Resources ./build/aireader-qt --endpoint mock://ai --model mock-medium
```

On Debian or Ubuntu the packages are those in [`docker/Dockerfile`](docker/Dockerfile).
`cmake --install build` puts the app, the bundled dictionary and a desktop
entry under the prefix.

Anywhere with Docker, `./build.sh` builds it in a Debian container, and

```sh
./build.sh script tools/smoke.txt "../References/Reader/Reader/Le Petit Prince (Saint-Exupéry, Antoine de).epub"
```

walks through every screen against the mock, offscreen, saving snapshots to
`build-docker/shots/`. The script's commands are listed in
[`src/App/SmokeScript.hpp`](src/App/SmokeScript.hpp).

### AppImage

`./build.sh appimage` builds `dist/AIReader-x86_64.AppImage` in an x86_64
Ubuntu 22.04 container (emulated on an ARM Mac, so it takes a while). Its
glibc, 2.35, is what sets the floor: it runs on Ubuntu 22.04 and later,
Mint 21, Debian 12, Fedora 36 and their peers. The recipe is
[`docker/Dockerfile.appimage`](docker/Dockerfile.appimage) and
[`docker/appimage.sh`](docker/appimage.sh):

- Pango is built from source, 1.50.14: the 1.50.6 that Ubuntu 22.04 ships
  puts space inside the words of a justified line.
- fribidi and HarfBuzz travel with the Pango built against them, though
  linuxdeploy would leave them to the host.
- gdk-pixbuf's decoders (GIF, TIFF…) are bundled with a template of their
  cache; the app writes the real cache on start, once it knows where it is
  mounted, and finds the bundled dictionary and the system's certificates
  the same way (`src/App/main.cpp`).

Like any AppImage it takes the desktop's own X11/xcb, OpenGL, fontconfig,
freetype and MIME database (`shared-mime-info`, which gdk-pixbuf needs to
tell a JPEG from a PNG). It draws through X11, or XWayland on a Wayland
desktop. `--appimage-extract-and-run` runs it where there is no FUSE.

It needs a GLib event loop — what Qt runs on Linux — since the shared code
hands work done on threads back through it; it says so and stops otherwise.

## Layout

| Folder | What lives there |
| --- | --- |
| `src/Common` | The navigator (a stack of screens), the screen with its header, a clickable row, and small helpers. |
| `src/Library`, `Reader`, `Lookup`, `Menu`, `Search`, `XRay`, `Chat`, `Words`, `Match`, `Settings`, `Dictionaries` | One `*View` per screen, each owning the Kindle app's `*Feature` for it and redrawing when it changes. |
| `src/App` | `main` and the smoke script. |
| `tools/smoke.txt` | The walk through every screen. |
| `docker`, `build.sh` | The Linux build without installing anything. |
| `packaging` | The desktop entry. |
