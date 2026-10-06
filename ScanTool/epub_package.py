"""Chapters bound into an EPUB 3, with an EPUB 2 table of contents too for
older readers — the same shape as the browser extension's books."""

import uuid
import zipfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List
from xml.sax.saxutils import escape

from book_flow import Chapter
from xhtml import attribute, chapter_document

MEDIA_TYPES = {".jpg": "image/jpeg", ".jpeg": "image/jpeg", ".png": "image/png", ".gif": "image/gif"}

STYLE = """body { line-height: 1.5; }
img { max-width: 100%; height: auto; }
p.picture { text-align: center; }
p.caption, p.label, aside { font-size: 0.85em; }
p.line { margin: 0.2em 0; }
aside { margin: 0.5em 0 1em; }
table { border-collapse: collapse; }
th, td { border: 1px solid; padding: 0.2em 0.5em; }
"""

CONTAINER = """<?xml version="1.0" encoding="utf-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>
"""


def chapter_title(chapter: Chapter, book_title: str) -> str:
    return chapter.title or chapter.label or book_title


def image_paths(chapters: List[Chapter]) -> Dict[Path, str]:
    """Each picture's path in the package; pages may reuse a file name."""
    paths: Dict[Path, str] = {}
    for chapter in chapters:
        for item in chapter.items:
            if item.kind == "image" and item.image not in paths:
                paths[item.image] = f"images/p{item.page}-{item.image.name}"
    return paths


def package_document(title: str, author: str, language: str, chapters: List[Chapter], images: Dict[Path, str], identifier: str) -> str:
    modified = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    items = [
        '<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>',
        '<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>',
        '<item id="style" href="style.css" media-type="text/css"/>',
    ]
    items += [f'<item id="chapter-{n}" href="text/chapter-{n}.xhtml" media-type="application/xhtml+xml"/>' for n in range(1, len(chapters) + 1)]
    # The first picture, usually the title page's, stands for the cover.
    for n, (source, href) in enumerate(images.items(), start=1):
        cover = ' properties="cover-image"' if n == 1 else ""
        items.append(f'<item id="image-{n}" href="{attribute(href)}" media-type="{MEDIA_TYPES[source.suffix.lower()]}"{cover}/>')
    spine = "\n".join(f'<itemref idref="chapter-{n}"/>' for n in range(1, len(chapters) + 1))
    creator = f"<dc:creator>{escape(author)}</dc:creator>\n" if author else ""
    cover = '<meta name="cover" content="image-1"/>\n' if images else ""
    return f"""<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="uid">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
<dc:identifier id="uid">{identifier}</dc:identifier>
<dc:title>{escape(title)}</dc:title>
{creator}<dc:language>{escape(language)}</dc:language>
<meta property="dcterms:modified">{modified}</meta>
{cover}</metadata>
<manifest>
{chr(10).join(items)}
</manifest>
<spine toc="ncx">
{spine}
</spine>
</package>
"""


def navigation(titles: List[str], title: str, language: str) -> str:
    entries = "\n".join(f'<li><a href="text/chapter-{n}.xhtml">{escape(name)}</a></li>' for n, name in enumerate(titles, start=1))
    lang = attribute(language)
    return f"""<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" lang="{lang}" xml:lang="{lang}">
<head><title>{escape(title)}</title></head>
<body><nav epub:type="toc"><h1>{escape(title)}</h1><ol>
{entries}
</ol></nav></body>
</html>
"""


def ncx(titles: List[str], title: str, identifier: str) -> str:
    points = "\n".join(
        f'<navPoint id="p{n}" playOrder="{n}"><navLabel><text>{escape(name)}</text></navLabel><content src="text/chapter-{n}.xhtml"/></navPoint>'
        for n, name in enumerate(titles, start=1)
    )
    return f"""<?xml version="1.0" encoding="utf-8"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
<head><meta name="dtb:uid" content="{identifier}"/></head>
<docTitle><text>{escape(title)}</text></docTitle>
<navMap>
{points}
</navMap>
</ncx>
"""


def write_epub(path: Path, title: str, author: str, language: str, chapters: List[Chapter]) -> None:
    identifier = f"urn:uuid:{uuid.uuid4()}"
    images = image_paths(chapters)
    titles = [chapter_title(chapter, title) for chapter in chapters]
    href = lambda item: f"../{images[item.image]}"
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as epub:
        # The mimetype comes first and uncompressed, so a reader can tell
        # the file from its first bytes.
        epub.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        epub.writestr("META-INF/container.xml", CONTAINER)
        epub.writestr("OEBPS/content.opf", package_document(title, author, language, chapters, images, identifier))
        epub.writestr("OEBPS/nav.xhtml", navigation(titles, title, language))
        epub.writestr("OEBPS/toc.ncx", ncx(titles, title, identifier))
        epub.writestr("OEBPS/style.css", STYLE)
        for n, (chapter, name) in enumerate(zip(chapters, titles), start=1):
            epub.writestr(f"OEBPS/text/chapter-{n}.xhtml", chapter_document(chapter, name, language, href))
        for source, inside in images.items():
            epub.write(source, f"OEBPS/{inside}")
