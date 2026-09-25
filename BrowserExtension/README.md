# Save as Book

A Chrome and Firefox extension that turns the page you are reading into an
EPUB for [AIReader](../README.md) — the article alone, without menus, share
buttons, comments or ads — and puts it in the WebDAV folder the apps sync
through. The iOS and Kindle apps fetch it on their next sync.

**Save as book.** One page, one book, saved at once. The title is the page's
heading, the author its byline or the site's name, and the language the one
the page declares, so lookups in the app know what they are reading.

**Book in progress.** *Add to book in progress* keeps the page aside instead;
add as many as you like, from as many sites, over as many days. Each page
becomes a chapter, in the order added. Give the book a title, remove a page
with `✕`, and *Save book* when it is complete. Until then the pages live in
the extension's own storage, with their pictures, so a page that later
changes or disappears is kept as it was.

With some text selected, either action takes the selection instead of
guessing where the article is.

Pictures are fetched and packed into the book. WebP, AVIF and SVG are
redrawn as JPEG or PNG and anything larger than 1600 pixels is scaled down,
since a Kindle shows neither the formats nor the size. Links are kept as
text: an e-reader cannot follow them, and a footnote number glued to a word
gets in the way of looking it up.

## Setting up

Load it unpacked:

- **Chrome:** `chrome://extensions`, turn on *Developer mode*, *Load
  unpacked*, and pick this folder.
- **Firefox (115 or later):** `about:debugging#/runtime/this-firefox`, *Load
  Temporary Add-on…*, and pick `manifest.json`. A temporary add-on is gone
  after a restart; to keep it, sign it through addons.mozilla.org.

Then open the extension's settings (*Settings* in its popup) and enter the
same WebDAV folder, user name and password as in the app's *Settings → Sync*.
Saving asks for access to all sites: to reach the server, and to fetch a
page's pictures wherever they are kept.

The password is kept in the extension's local storage, which the browser does
not encrypt. A WebDAV account used only for AIReader is the sensible choice.

## Where things go

Books are saved as `Books/<author> - <title>.epub` in the folder, with
` (2)` and so on added rather than replacing a book already there. The apps
treat that folder as the shared library: a book added to either app goes
there too, and removing a book in an app removes it from that device only.
To delete a book for good, delete its file from the server.

## Layout

No build step and no dependencies; the manifest serves both browsers.

| File | What it does |
| --- | --- |
| `extract.js` | Injected into the page: finds the main text, strips the rest, returns XHTML and the pictures' addresses. |
| `lib/capture.js` | Fetches and converts the pictures, and points the markup at the copies. |
| `lib/epub.js`, `lib/zip.js` | Bind pages into an EPUB 3 (with an EPUB 2 table of contents too). |
| `lib/names.js` | The file-name rule, the same as the apps' `RemoteBookName`. |
| `lib/webdav.js` | Lists the books folder and stores a book, making folders as needed. |
| `lib/books.js` | The two ways a page becomes a book. |
| `lib/storage.js` | Settings and the book in progress. |
| `popup.*`, `options.*` | The toolbar popup and the settings page. |
