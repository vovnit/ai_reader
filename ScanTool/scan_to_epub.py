"""Makes an EPUB of a scanned book from what Mistral's OCR exported for it.

    python3 scan_to_epub.py path/to/export --language fr --author "Domitille Hatuel"
"""

import argparse
import re
import sys
from pathlib import Path
from typing import List, Optional

from book_flow import compose
from epub_package import chapter_title, write_epub
from ocr_export import read_pages


def file_name(title: str, author: str) -> str:
    # The shared Books folder's rule, roughly: author and title, nothing a
    # FAT partition refuses.
    stem = f"{author} - {title}" if author else title
    return " ".join(re.sub(r'[/\\:*?"<>|\x00-\x1f]', " ", stem).split()).strip(". ") + ".epub"


def main(arguments: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Make an EPUB of a scanned book from its Mistral OCR export.")
    parser.add_argument("folder", type=Path, help="the export: the folder holding pages/page-N/page-metadata.json")
    parser.add_argument("--language", required=True, help="the book's language, such as fr or de, so lookups know what they read")
    parser.add_argument("--title", help="the book's title; otherwise its first heading")
    parser.add_argument("--author", default="", help="the book's author")
    parser.add_argument("--output", type=Path, help="where to write it; otherwise beside the export, named like the shared Books folder's files")
    options = parser.parse_args(arguments)

    try:
        pages = read_pages(options.folder)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    chapters = compose(pages, options.title)
    title = options.title or next((chapter.title for chapter in chapters if chapter.title), options.folder.name)
    output = options.output or options.folder.parent / file_name(title, options.author)
    write_epub(output, title, options.author, options.language, chapters)

    print(f"{len(pages)} pages, {len(chapters)} chapters: {output}", file=sys.stderr)
    for chapter in chapters:
        print(f"  {chapter_title(chapter, title)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
