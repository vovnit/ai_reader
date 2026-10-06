"""A chapter as an XHTML document: the OCR's bits of Markdown — emphasis,
headings, pipe tables — turned into elements, and each footnote placed
after the paragraph that refers to it."""

import re
from typing import Callable, List
from xml.sax.saxutils import escape

from book_flow import CLOSE, OPEN, Chapter, Item

STRONG = re.compile(r"\*\*(.+?)\*\*")
EMPHASIS = re.compile(r"(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])")
REFERENCE = re.compile(f"{OPEN}([^{CLOSE}]*){CLOSE}")
DIVIDER = re.compile(r"^[\s|:-]+$")
MATH = re.compile(r"\\\((.*?)\\\)|\$([^$]*\\[a-zA-Z]+[^$]*)\$")
COMMAND = re.compile(r"\\([a-zA-Z]+)\s*")
# What the OCR writes as LaTeX in a page that is not about mathematics:
# bullets, arrows and boxes to tick.
SYMBOLS = {"triangleright": "▸", "rightarrow": "→", "to": "→", "square": "☐", "checkmark": "✓", "times": "×", "bullet": "•"}


def attribute(text: str) -> str:
    return escape(text, {'"': "&quot;"})


def inline(text: str) -> str:
    """Text with its emphasis as elements. A note reference is a link the
    apps drop, so that its number is never taken for part of a word."""
    text = MATH.sub(lambda match: COMMAND.sub(lambda command: SYMBOLS.get(command[1], ""), match[1] or match[2]).strip(), text)
    html = escape(text)
    html = STRONG.sub(r"<strong>\1</strong>", html)
    html = EMPHASIS.sub(r"<em>\1</em>", html)
    return REFERENCE.sub(
        lambda match: f'<a epub:type="noteref" href="#{match[1]}"><sup>{match[1].rsplit("-", 1)[1]}</sup></a>', html
    )


def table(markdown: str) -> str:
    rows = [row.strip().strip("|").split("|") for row in markdown.splitlines() if row.strip() and not DIVIDER.match(row)]
    if not rows:
        return ""
    head = "".join(f"<th>{inline(cell.strip())}</th>" for cell in rows[0])
    body = "".join("<tr>" + "".join(f"<td>{inline(cell.strip())}</td>" for cell in row) + "</tr>" for row in rows[1:])
    return f"<table><thead><tr>{head}</tr></thead><tbody>{body}</tbody></table>"


def note(item: Item, referred: bool) -> str:
    # A referred note is a footnote a reader may show where it is tapped;
    # the apps show it in its place, right after its paragraph.
    kind = ' epub:type="footnote"' if referred else ""
    return f'<aside id="{attribute(item.id)}"{kind}><p>{inline(item.text)}</p></aside>'


def element(item: Item, image_href: Callable[[Item], str]) -> str:
    if item.kind == "heading":
        return f"<h{item.level}>{inline(item.text)}</h{item.level}>"
    if item.kind == "label":
        return f'<p class="label">{inline(item.text)}</p>'
    if item.kind == "paragraph":
        return f"<p>{inline(item.text)}</p>"
    if item.kind == "lines":
        return "\n".join(f'<p class="line">{inline(line)}</p>' for line in item.lines)
    if item.kind == "table":
        return table(item.text)
    if item.kind == "image":
        return f'<p class="picture"><img src="{attribute(image_href(item))}" alt=""/></p>'
    if item.kind == "caption":
        return f'<p class="caption">{inline(item.text)}</p>'
    if item.kind == "note":
        return note(item, referred=False)
    raise ValueError(f"unknown item {item.kind}")


def chapter_document(chapter: Chapter, title: str, language: str, image_href: Callable[[Item], str]) -> str:
    parts: List[str] = []
    for item in chapter.items:
        parts.append(element(item, image_href))
        parts.extend(note(attached, referred=True) for attached in item.notes)
    lang = attribute(language)
    return f"""<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" lang="{lang}" xml:lang="{lang}">
<head><title>{escape(title)}</title><link rel="stylesheet" type="text/css" href="../style.css"/></head>
<body>
{chr(10).join(part for part in parts if part)}
</body>
</html>
"""
