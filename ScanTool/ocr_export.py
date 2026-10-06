"""Reads what Mistral's OCR exports for a scanned book: a folder with
`pages/page-N/page-metadata.json` for each page, listing the page's blocks
with their kind and place, and the page's pictures beside it."""

import json
from dataclasses import dataclass
from pathlib import Path
from typing import List


@dataclass
class Block:
    """One block of a page. `kind` is the OCR's: title, text, list, table,
    image, caption, header, footer or references. The box is in fractions
    of the page, so pages scanned at different sizes compare."""

    kind: str
    content: str
    page: int
    left: float = 0.0
    top: float = 0.0
    right: float = 1.0
    bottom: float = 1.0
    folder: Path = Path(".")


def page_number(path: Path) -> int:
    return int(path.parent.name.rsplit("-", 1)[1])


def read_pages(folder: Path) -> List[List[Block]]:
    """Every page's blocks, in page order."""
    paths = sorted((folder / "pages").glob("page-*/page-metadata.json"), key=page_number)
    if not paths:
        raise ValueError(f"{folder} has no pages/page-N/page-metadata.json")
    pages = []
    for path in paths:
        data = json.loads(path.read_text(encoding="utf-8"))
        width = data["dimensions"]["width"]
        height = data["dimensions"]["height"]
        pages.append([
            Block(
                kind=block["type"],
                content=block["content"],
                page=page_number(path),
                left=block["topLeftX"] / width,
                top=block["topLeftY"] / height,
                right=block["bottomRightX"] / width,
                bottom=block["bottomRightY"] / height,
                folder=path.parent,
            )
            for block in data["blocks"]
        ])
    return pages
