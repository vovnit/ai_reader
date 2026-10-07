import posixpath
import urllib.parse
import zipfile
from dataclasses import dataclass
from html.parser import HTMLParser
from pathlib import Path
from typing import List
from xml.etree import ElementTree

NAMESPACES = {
    "container": "urn:oasis:names:tc:opendocument:xmlns:container",
    "opf": "http://www.idpf.org/2007/opf",
    "dc": "http://purl.org/dc/elements/1.1/",
}

# Elements that end a paragraph, so words on either side never share an
# example.
BLOCK_ELEMENTS = {
    "address", "blockquote", "br", "dd", "div", "dt", "figcaption",
    "h1", "h2", "h3", "h4", "h5", "h6", "hr", "li", "p", "pre",
    "section", "td", "th", "tr",
}
SKIPPED_ELEMENTS = {"head", "script", "style"}


class BookTextError(Exception):
    pass


@dataclass(frozen=True)
class BookText:
    title: str
    author: str
    paragraphs: List[str]


class _Paragraphs(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.paragraphs: List[str] = []
        self._pieces: List[str] = []
        self._skipping = 0

    def handle_starttag(self, tag, attrs) -> None:
        if tag in SKIPPED_ELEMENTS:
            self._skipping += 1
        elif tag in BLOCK_ELEMENTS:
            self._end_paragraph()

    def handle_endtag(self, tag) -> None:
        if tag in SKIPPED_ELEMENTS:
            self._skipping = max(0, self._skipping - 1)
        elif tag in BLOCK_ELEMENTS:
            self._end_paragraph()

    def handle_data(self, data) -> None:
        if not self._skipping:
            self._pieces.append(data)

    def close(self) -> None:
        super().close()
        self._end_paragraph()

    def _end_paragraph(self) -> None:
        text = " ".join("".join(self._pieces).split())
        if text:
            self.paragraphs.append(text)
        self._pieces = []


def read_epub(path: Path) -> BookText:
    """The book's paragraphs in reading order, with its title and author."""
    try:
        with zipfile.ZipFile(str(path)) as archive:
            container = ElementTree.fromstring(archive.read("META-INF/container.xml"))
            package_path = container.find(".//container:rootfile", NAMESPACES).get("full-path")
            package = ElementTree.fromstring(archive.read(package_path))
            base = posixpath.dirname(package_path)
            manifest = {
                item.get("id"): item.get("href")
                for item in package.iterfind("opf:manifest/opf:item", NAMESPACES)
            }

            paragraphs: List[str] = []
            for itemref in package.iterfind("opf:spine/opf:itemref", NAMESPACES):
                href = manifest.get(itemref.get("idref"))
                if not href:
                    continue
                name = posixpath.normpath(posixpath.join(base, urllib.parse.unquote(href)))
                parser = _Paragraphs()
                parser.feed(archive.read(name).decode("utf-8", "replace"))
                parser.close()
                paragraphs.extend(parser.paragraphs)
    except (OSError, KeyError, AttributeError, zipfile.BadZipFile, ElementTree.ParseError) as error:
        raise BookTextError("{} is not an EPUB this tool can read ({})".format(path, error))

    return BookText(
        title=_metadata(package, "title") or path.stem,
        author=_metadata(package, "creator"),
        paragraphs=paragraphs,
    )


def _metadata(package: ElementTree.Element, name: str) -> str:
    element = package.find("opf:metadata/dc:{}".format(name), NAMESPACES)
    return " ".join((element.text or "").split()) if element is not None else ""
