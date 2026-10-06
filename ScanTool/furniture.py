"""What on a scanned page is not the book's text: running heads, page
numbers, the lettering of a picture read as words, the printed table of
contents, and the corner of the facing page's picture."""

import re
from typing import List

from ocr_export import Block

NUMBER = re.compile(r"^\s*\d{1,4}\s*$")
ROMAN = re.compile(r"^\s*[ivxlcdm]{1,7}\s*$", re.IGNORECASE)
ENDS_IN_NUMBER = re.compile(r"\d\s*$")


def covers_page(block: Block) -> bool:
    # A full-page picture comes back as one block the size of the page,
    # holding whatever letters were painted in it.
    return block.right - block.left > 0.9 and block.bottom - block.top > 0.9


def is_sliver(block: Block) -> bool:
    # A picture printed across both pages of a spread leaves its edge in
    # the corner of the facing page.
    at_edge = min(block.left, block.top, 1 - block.right, 1 - block.bottom) < 0.01
    return block.kind == "image" and at_edge and block.right - block.left < 0.2


def is_contents(page: List[Block]) -> bool:
    """The printed table of contents: lines that mostly end in page
    numbers, which mean nothing in an EPUB with its own."""
    lines = [line for block in page if block.kind in ("text", "list") for line in block.content.splitlines() if line.strip()]
    numbered = [line for line in lines if ENDS_IN_NUMBER.search(line)]
    return len(lines) >= 5 and len(numbered) * 3 >= len(lines) * 2


def is_furniture(block: Block) -> bool:
    if block.kind == "header":
        return True
    if block.kind == "footer":
        # Footnotes come as footers too; only a bare number is the page's.
        return bool(NUMBER.match(block.content) or ROMAN.match(block.content))
    if block.kind == "image":
        return is_sliver(block)
    # A lone number: a page number the OCR did not mark, or an audio track's.
    return covers_page(block) or bool(NUMBER.match(block.content))


def kept_blocks(page: List[Block]) -> List[Block]:
    """The page's blocks without its furniture; none of a contents page."""
    if is_contents(page):
        return []
    return [block for block in page if not is_furniture(block)]
