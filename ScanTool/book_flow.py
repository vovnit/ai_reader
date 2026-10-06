"""The scanned pages as a book: chapters of headings, paragraphs and
pictures, with what the page breaks did undone. A sentence cut by a page
or a picture is joined again, a running head that repeats the book's or
the chapter's title is dropped, and a footnote follows the paragraph that
refers to it."""

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional

from furniture import kept_blocks
from ocr_export import Block

LABEL = re.compile(r"^(chapitre|chapter|capítulo|capitolo|kapitel|hoofdstuk|rozdział|глава)\s+\S+$", re.IGNORECASE)
IMAGE = re.compile(r"!\[[^\]]*\]\(([^)]+)\)")
MARKER = re.compile(r"\s*(?:\$\^\{?(\d+)\}?\$|([⁰¹²³⁴⁵⁶⁷⁸⁹]+))")
NOTE = re.compile(r"^\s*(\d+)\s*[.)]?\s+(.+)$")
FINISHED = re.compile(r"[.!?…:;»\"”)\]]\s*$")
SUPERSCRIPT = str.maketrans("⁰¹²³⁴⁵⁶⁷⁸⁹", "0123456789")

# A note reference, until the page is written: the note's id between two
# characters no text has.
OPEN, CLOSE = "", ""


@dataclass
class Item:
    """`kind` is heading, label, paragraph, lines, table, image, caption or
    note; `page` is where it ends."""

    kind: str
    text: str = ""
    page: int = 0
    level: int = 1
    lines: List[str] = field(default_factory=list)
    image: Optional[Path] = None
    notes: List["Item"] = field(default_factory=list)
    id: str = ""


@dataclass
class Chapter:
    title: str = ""
    label: str = ""
    items: List[Item] = field(default_factory=list)


def plain(text: str) -> str:
    """Text as compared with a title: no markup, case or spacing."""
    return " ".join(re.sub(r"[#*_]", " ", text).split()).casefold()


def heading(block: Block) -> Item:
    level = len(block.content) - len(block.content.lstrip("#"))
    text = " ".join(block.content.replace("#", " ").split())
    return Item("heading", text=text.strip("*_ ") or text, page=block.page, level=min(max(level, 1), 6))


def page_notes(blocks: List[Block]) -> Dict[str, Item]:
    """The page's footnotes by number; one without a number by its order."""
    notes: Dict[str, Item] = {}
    for block in blocks:
        if block.kind not in ("footer", "references"):
            continue
        for line in block.content.splitlines():
            match = NOTE.match(line)
            if match:
                number, text = match.groups()
            elif notes and line.strip():
                last = list(notes.values())[-1]
                last.text += " " + line.strip()
                continue
            else:
                number, text = f"_{len(notes)}", line.strip()
            notes[number] = Item("note", text=text, page=block.page, id=f"n{block.page}-{number}")
    return notes


def paragraphs(block: Block) -> List[str]:
    """A text block's paragraphs; a single line break is the scan's wrap."""
    return [" ".join(part.split()) for part in re.split(r"\n\s*\n", block.content) if part.strip()]


def is_open(chapter: Chapter) -> Optional[Item]:
    """The paragraph a page or a picture cut short, if the chapter ends in one."""
    for item in reversed(chapter.items):
        if item.kind in ("image", "caption", "note"):
            continue
        unmarked = re.sub(f"{OPEN}[^{CLOSE}]*{CLOSE}", "", item.text)
        return item if item.kind == "paragraph" and not FINISHED.search(unmarked) else None
    return None


class Composer:
    def __init__(self, title: Optional[str]):
        # The book's title, given or its first heading; its first
        # appearance is the title page, any other a running head.
        self.title = plain(title) if title else None
        self.title_seen = False
        # Once a chapter has been labelled, a top-level heading no longer
        # opens one: it is a section of the chapter, such as its exercises.
        self.chapter_begun = False
        self.chapters: List[Chapter] = []

    @property
    def chapter(self) -> Chapter:
        if not self.chapters:
            self.chapters.append(Chapter())
        return self.chapters[-1]

    def open_chapter(self, page: int) -> Chapter:
        # The picture above a chapter's opening heading belongs to it.
        moved = []
        while self.chapters and self.chapter.items and self.chapter.items[-1].kind == "image" and self.chapter.items[-1].page == page:
            moved.insert(0, self.chapter.items.pop())
        self.chapters.append(Chapter(items=moved))
        return self.chapters[-1]

    def is_running_head(self, text: str) -> bool:
        if plain(text) in (plain(self.chapter.label), plain(self.chapter.title)):
            return True
        if plain(text) != self.title:
            return False
        repeated = self.title_seen
        self.title_seen = True
        return repeated

    def add_heading(self, item: Item) -> None:
        if self.title is None:
            self.title = plain(item.text)
        if self.is_running_head(item.text):
            return
        if LABEL.match(item.text):
            self.add_label(item)
        elif self.chapters and not self.chapter.title:
            self.chapter.title = item.text
            self.chapter.items.append(item)
        elif item.level == 1 and not self.chapter_begun:
            self.open_chapter(item.page).title = item.text
            self.chapter.items.append(item)
        else:
            self.chapter.items.append(item)

    def add_label(self, item: Item) -> None:
        self.chapter_begun = True
        chapter = self.open_chapter(item.page)
        chapter.label = item.text
        chapter.items.append(Item("label", text=item.text, page=item.page))

    def add_paragraph(self, text: str, page: int, notes: Dict[str, Item]) -> None:
        if len(text) < 80 and self.is_running_head(text):
            return
        if LABEL.match(text):
            self.add_label(Item("label", text=text, page=page))
            return
        open_paragraph = is_open(self.chapter)
        # Text starting in lower case goes on with the sentence; so does any
        # word, a name say, after a page break that cut a long paragraph.
        continues = open_paragraph is not None and (
            text[:1].islower() or (open_paragraph.page < page and len(open_paragraph.text) >= 100 and text[:1].isalpha())
        )
        target = open_paragraph if continues else Item("paragraph", page=page)
        text = self.mark_notes(text, target, notes)
        if continues:
            glue = "" if target.text.endswith("-") else " "
            target.text += glue + text
            target.page = page
        else:
            target.text = text
            self.chapter.items.append(target)

    def mark_notes(self, text: str, paragraph: Item, notes: Dict[str, Item]) -> str:
        def replace(match: re.Match) -> str:
            number = (match.group(1) or match.group(2)).translate(SUPERSCRIPT)
            note = notes.pop(number, None)
            if note is None:
                return ""
            paragraph.notes.append(note)
            return f"{OPEN}{note.id}{CLOSE}"

        return MARKER.sub(replace, text)

    def add_page(self, blocks: List[Block]) -> None:
        notes = page_notes(blocks)
        for block in blocks:
            if block.kind in ("footer", "references"):
                continue
            if block.kind == "title":
                self.add_heading(heading(block))
            elif block.kind == "text":
                for text in paragraphs(block):
                    self.add_paragraph(text, block.page, notes)
            elif block.kind == "list":
                item = Item("lines", page=block.page)
                item.lines = [self.mark_notes(line.strip(), item, notes) for line in block.content.splitlines() if line.strip()]
                self.chapter.items.append(item)
            elif block.kind == "table":
                self.chapter.items.append(Item("table", text=block.content, page=block.page))
            elif block.kind == "caption":
                self.chapter.items.append(Item("caption", text=" ".join(block.content.split()), page=block.page))
            elif block.kind == "image":
                for name in IMAGE.findall(block.content):
                    # Only what every e-reader shows; the OCR writes JPEG.
                    if (block.folder / name).is_file() and Path(name).suffix.lower() in (".jpg", ".jpeg", ".png", ".gif"):
                        self.chapter.items.append(Item("image", image=block.folder / name, page=block.page))
        # A note whose mark the OCR missed stays where the page had it.
        self.chapter.items.extend(notes.values())


def compose(pages: List[List[Block]], title: Optional[str] = None) -> List[Chapter]:
    """The book's chapters. A book that labels its chapters (Chapitre 1,
    Chapter 2…) starts one at each label; before the first, and in a book
    without labels, each top-level heading starts one."""
    composer = Composer(title)
    for page in pages:
        composer.add_page(kept_blocks(page))
    return [chapter for chapter in composer.chapters if chapter.items]
