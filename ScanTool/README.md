# Scan to EPUB

Makes an EPUB for [AIReader](../README.md) out of a scanned book, from what
[Mistral's OCR](https://mistral.ai/news/mistral-ocr) exports for it: a
folder with `pages/page-N/page-metadata.json` for every page, listing the
page's blocks — title, text, list, table, image, caption, header, footer —
with where each sits, and the page's pictures beside it.

```bash
python3 scan_to_epub.py path/to/export --language fr --author "Domitille Hatuel"
```

The book is written beside the export folder, as `<author> - <title>.epub`
like the shared `Books` folder's files, or wherever `--output` says. The
title is the book's first heading unless `--title` gives it. `--language`
is required: lookups in the app need to know what they are reading. The
script needs nothing beyond Python's standard library.

## What it undoes

A scan is laid out for paper. What the page did to the text is undone:

- **Page furniture goes.** Running heads (the OCR's headers, and any
  heading that repeats the book's title or the chapter's), page numbers,
  the printed table of contents, and the lettering of a full-page picture
  that the OCR read as words. So does the corner of a picture printed across
  a spread, which shows on the facing page.
- **Sentences are whole again.** A paragraph cut by a page or a picture is
  joined to the text that goes on with it: text starting in lower case, or,
  after a page break that cut a long paragraph short, any word — a name.
  The picture stays where it was, after the joined paragraph.
- **Footnotes follow their paragraph.** The note's number becomes a link
  marked as a note reference, which the apps drop rather than read as part
  of the word, and the note is set right after the paragraph — where a
  learner wants a gloss. Readers that show footnotes in a popup do so.
  A note whose mark the OCR missed stays at the end of its page.
- **Chapters are the book's.** A book that labels its chapters (*Chapitre
  1*, *Chapter 2*…) starts one at each label, titled by the heading after
  it, with the picture above the label. What comes between — exercises,
  background pages — stays with the chapter before, as in print. Before the
  first label, and in a book without labels, each top-level heading starts
  a chapter.

The first picture is the cover. Emphasis, pipe tables and the few LaTeX
symbols an OCR uses for bullets and boxes become plain elements.

## Layout

| File | What it does |
| --- | --- |
| `scan_to_epub.py` | The command line. |
| `ocr_export.py` | Reads the export into pages of blocks. |
| `furniture.py` | Tells the page's furniture from the book's text. |
| `book_flow.py` | Makes chapters of the kept blocks: joins, running heads, footnotes. |
| `xhtml.py` | A chapter as an XHTML document. |
| `epub_package.py` | Binds the chapters into an EPUB 3 with an EPUB 2 table of contents. |

Tests run from this folder:

```bash
python3 -m unittest discover -s tests -t .
```
