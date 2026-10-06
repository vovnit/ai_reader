// Writes a book of plain chapters, `{ title, paragraphs: [{ text, heading }] }`,
// as an EPUB 3 with an EPUB 2 table of contents too — the shape of
// ScanTool's books and the browser extension's, and of the Kindle app's
// `EpubBuilder`.

const escape = (text) => text.replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;").replaceAll('"', "&quot;");
const chapterPath = (number) => `text/chapter-${number}.xhtml`;

const container = `<?xml version="1.0" encoding="utf-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>
`;

function packageDocument(book, count) {
  let items = '<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>\n'
    + '<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>\n'
    + '<item id="style" href="style.css" media-type="text/css"/>\n';
  let spine = "";
  for (let n = 1; n <= count; n++) {
    items += `<item id="chapter-${n}" href="${chapterPath(n)}" media-type="application/xhtml+xml"/>\n`;
    spine += `<itemref idref="chapter-${n}"/>\n`;
  }
  const creator = book.author ? `<dc:creator>${escape(book.author)}</dc:creator>\n` : "";
  return `<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="uid">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
<dc:identifier id="uid">${escape(book.identifier)}</dc:identifier>
<dc:title>${escape(book.title)}</dc:title>
${creator}<dc:language>${escape(book.language)}</dc:language>
<meta property="dcterms:modified">${book.modified}</meta>
</metadata>
<manifest>
${items}</manifest>
<spine toc="ncx">
${spine}</spine>
</package>
`;
}

function opening(language, title) {
  const lang = escape(language);
  return `<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" lang="${lang}" xml:lang="${lang}">
<head><title>${escape(title)}</title>`;
}

function navigation(book, chapters) {
  const entries = chapters.map((chapter, i) => `<li><a href="${chapterPath(i + 1)}">${escape(chapter.title)}</a></li>\n`).join("");
  return `${opening(book.language, book.title)}</head>
<body><nav epub:type="toc"><h1>${escape(book.title)}</h1><ol>
${entries}</ol></nav></body>
</html>
`;
}

function ncx(book, chapters) {
  const points = chapters.map((chapter, i) => `<navPoint id="p${i + 1}" playOrder="${i + 1}"><navLabel><text>${escape(chapter.title)}</text></navLabel>`
    + `<content src="${chapterPath(i + 1)}"/></navPoint>\n`).join("");
  return `<?xml version="1.0" encoding="utf-8"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
<head><meta name="dtb:uid" content="${escape(book.identifier)}"/></head>
<docTitle><text>${escape(book.title)}</text></docTitle>
<navMap>
${points}</navMap>
</ncx>
`;
}

function chapterDocument(book, chapter) {
  const body = chapter.paragraphs.map(({ text, heading }) => (heading ? `<h2>${escape(text)}</h2>\n` : `<p>${escape(text)}</p>\n`)).join("");
  return `${opening(book.language, chapter.title)}<link rel="stylesheet" type="text/css" href="../style.css"/></head>
<body>
${body}</body>
</html>
`;
}

/** `[path, contents]` for each of the book's files, `mimetype` first. `book` is `{ title, author, language, identifier, modified }`. */
export function epubFiles(book, chapters) {
  return [
    ["mimetype", "application/epub+zip"],
    ["META-INF/container.xml", container],
    ["OEBPS/content.opf", packageDocument(book, chapters.length)],
    ["OEBPS/nav.xhtml", navigation(book, chapters)],
    ["OEBPS/toc.ncx", ncx(book, chapters)],
    ["OEBPS/style.css", "body { line-height: 1.5; }\n"],
    ...chapters.map((chapter, i) => [`OEBPS/${chapterPath(i + 1)}`, chapterDocument(book, chapter)]),
  ];
}
