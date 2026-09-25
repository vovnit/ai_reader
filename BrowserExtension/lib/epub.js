// Web pages bound into an EPUB 3 that both AIReader apps read: one chapter
// per page, with an EPUB 2 table of contents too, for older readers.
import { ZipWriter } from "./zip.js";

export function escapeXML(text) {
  return String(text)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

const style = `body { line-height: 1.5; }
p.source { font-size: 0.85em; font-style: italic; }
img { max-width: 100%; height: auto; }
figure { margin: 1em 0; }
figcaption { font-size: 0.85em; }
pre { white-space: pre-wrap; }
`;

function chapter(page, language) {
  const source = [page.site, page.byline].filter(Boolean).join(" · ");
  return `<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" lang="${escapeXML(language)}" xml:lang="${escapeXML(language)}">
<head><title>${escapeXML(page.title)}</title><link rel="stylesheet" type="text/css" href="../style.css"/></head>
<body>
<h1>${escapeXML(page.title)}</h1>
${source ? `<p class="source">${escapeXML(source)}</p>\n` : ""}${page.xhtml}
</body>
</html>
`;
}

function navigation(pages, title, language) {
  const items = pages
    .map((page, index) => `<li><a href="text/page-${index + 1}.xhtml">${escapeXML(page.title)}</a></li>`)
    .join("\n");
  return `<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" lang="${escapeXML(language)}" xml:lang="${escapeXML(language)}">
<head><title>${escapeXML(title)}</title></head>
<body><nav epub:type="toc"><h1>${escapeXML(title)}</h1><ol>
${items}
</ol></nav></body>
</html>
`;
}

function ncx(pages, title, identifier) {
  const points = pages
    .map((page, index) => `<navPoint id="p${index + 1}" playOrder="${index + 1}"><navLabel><text>${escapeXML(page.title)}</text></navLabel><content src="text/page-${index + 1}.xhtml"/></navPoint>`)
    .join("\n");
  return `<?xml version="1.0" encoding="utf-8"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
<head><meta name="dtb:uid" content="${escapeXML(identifier)}"/></head>
<docTitle><text>${escapeXML(title)}</text></docTitle>
<navMap>
${points}
</navMap>
</ncx>
`;
}

function packageDocument(book, identifier, modified) {
  const items = [
    `<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>`,
    `<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>`,
    `<item id="style" href="style.css" media-type="text/css"/>`,
    ...book.pages.map((_, index) => `<item id="page-${index + 1}" href="text/page-${index + 1}.xhtml" media-type="application/xhtml+xml"/>`),
    ...book.images.map((image, index) => `<item id="image-${index + 1}" href="${escapeXML(image.path)}" media-type="${escapeXML(image.type)}"/>`),
  ];
  const spine = book.pages.map((_, index) => `<itemref idref="page-${index + 1}"/>`);
  const creator = book.author ? `<dc:creator>${escapeXML(book.author)}</dc:creator>\n` : "";
  const source = book.pages.length === 1 && book.pages[0].url ? `<dc:source>${escapeXML(book.pages[0].url)}</dc:source>\n` : "";
  return `<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="uid">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
<dc:identifier id="uid">${escapeXML(identifier)}</dc:identifier>
<dc:title>${escapeXML(book.title)}</dc:title>
${creator}<dc:language>${escapeXML(book.language)}</dc:language>
${source}<meta property="dcterms:modified">${modified}</meta>
</metadata>
<manifest>
${items.join("\n")}
</manifest>
<spine toc="ncx">
${spine.join("\n")}
</spine>
</package>
`;
}

/**
 * The book as an `.epub` blob.
 *
 * `book` is `{ title, author, language, pages, images }`: each page
 * `{ title, site, byline, url, xhtml }`, its `xhtml` a body fragment whose
 * images point at `../images/…`; each image `{ path, type, bytes }`, its
 * path relative to the package, `images/…`.
 */
export async function buildEpub(book) {
  const language = book.language || "und";
  const identifier = `urn:uuid:${crypto.randomUUID()}`;
  const modified = new Date().toISOString().replace(/\.\d+Z$/, "Z");
  const zip = new ZipWriter();
  await zip.add("mimetype", "application/epub+zip", { compress: false });
  await zip.add(
    "META-INF/container.xml",
    `<?xml version="1.0" encoding="utf-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>
`
  );
  await zip.add("OEBPS/content.opf", packageDocument({ ...book, language }, identifier, modified));
  await zip.add("OEBPS/nav.xhtml", navigation(book.pages, book.title, language));
  await zip.add("OEBPS/toc.ncx", ncx(book.pages, book.title, identifier));
  await zip.add("OEBPS/style.css", style);
  for (const [index, page] of book.pages.entries()) {
    await zip.add(`OEBPS/text/page-${index + 1}.xhtml`, chapter(page, page.language || language));
  }
  for (const image of book.images) {
    await zip.add(`OEBPS/${image.path}`, image.bytes);
  }
  return zip.finish("application/epub+zip");
}
