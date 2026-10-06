import Foundation

/// A chapter to be written into an EPUB: its title for the table of
/// contents, and its paragraphs, some of them headings.
struct ChapterDraft: Equatable, Sendable {
    struct Paragraph: Equatable, Sendable {
        var text: String
        var heading = false
    }

    var title: String
    var paragraphs: [Paragraph] = []
}

/// Writes a book of plain chapters as an EPUB 3 with an EPUB 2 table of
/// contents too — the shape of ScanTool's books and the browser
/// extension's, and of the Kindle app's `EpubBuilder`.
enum EPUBBuilder {
    struct Metadata: Equatable, Sendable {
        var title: String
        var author: String?
        var language: String
        var identifier: String
        /// When the book was made, as `2026-10-06T12:00:00Z`.
        var modified: String
    }

    /// The path, inside the book, of its package document.
    static let packagePath = "OEBPS/content.opf"

    /// The book's files by their path, `mimetype` first.
    static func files(_ book: Metadata, chapters: [ChapterDraft]) -> [(path: String, contents: String)] {
        var files = [
            (path: "mimetype", contents: "application/epub+zip"),
            (path: "META-INF/container.xml", contents: container),
            (path: packagePath, contents: package(book, count: chapters.count)),
            (path: "OEBPS/nav.xhtml", contents: navigation(book, chapters)),
            (path: "OEBPS/toc.ncx", contents: ncx(book, chapters)),
            (path: "OEBPS/style.css", contents: "body { line-height: 1.5; }\n"),
        ]
        for (index, chapter) in chapters.enumerated() {
            files.append((path: "OEBPS/\(chapterPath(index + 1))", contents: document(book, chapter)))
        }
        return files
    }

    private static func escape(_ text: String) -> String {
        text.replacingOccurrences(of: "&", with: "&amp;")
            .replacingOccurrences(of: "<", with: "&lt;")
            .replacingOccurrences(of: ">", with: "&gt;")
            .replacingOccurrences(of: "\"", with: "&quot;")
    }

    private static func chapterPath(_ number: Int) -> String { "text/chapter-\(number).xhtml" }

    private static let container = """
        <?xml version="1.0" encoding="utf-8"?>
        <container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
        <rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
        </container>

        """

    private static func package(_ book: Metadata, count: Int) -> String {
        var items = "<item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>\n"
            + "<item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>\n"
            + "<item id=\"style\" href=\"style.css\" media-type=\"text/css\"/>\n"
        var spine = ""
        for n in stride(from: 1, through: count, by: 1) {
            items += "<item id=\"chapter-\(n)\" href=\"\(chapterPath(n))\" media-type=\"application/xhtml+xml\"/>\n"
            spine += "<itemref idref=\"chapter-\(n)\"/>\n"
        }
        let creator = book.author.map { $0.isEmpty ? "" : "<dc:creator>\(escape($0))</dc:creator>\n" } ?? ""
        return """
            <?xml version="1.0" encoding="utf-8"?>
            <package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="uid">
            <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
            <dc:identifier id="uid">\(escape(book.identifier))</dc:identifier>
            <dc:title>\(escape(book.title))</dc:title>
            \(creator)<dc:language>\(escape(book.language))</dc:language>
            <meta property="dcterms:modified">\(book.modified)</meta>
            </metadata>
            <manifest>
            \(items)</manifest>
            <spine toc="ncx">
            \(spine)</spine>
            </package>

            """
    }

    private static func opening(_ language: String, title: String) -> String {
        let lang = escape(language)
        return """
            <?xml version="1.0" encoding="utf-8"?>
            <!DOCTYPE html>
            <html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" lang="\(lang)" xml:lang="\(lang)">
            <head><title>\(escape(title))</title>
            """
    }

    private static func navigation(_ book: Metadata, _ chapters: [ChapterDraft]) -> String {
        let entries = chapters.enumerated().map { "<li><a href=\"\(chapterPath($0.offset + 1))\">\(escape($0.element.title))</a></li>\n" }.joined()
        return opening(book.language, title: book.title) + """
            </head>
            <body><nav epub:type="toc"><h1>\(escape(book.title))</h1><ol>
            \(entries)</ol></nav></body>
            </html>

            """
    }

    private static func ncx(_ book: Metadata, _ chapters: [ChapterDraft]) -> String {
        let points = chapters.enumerated().map {
            "<navPoint id=\"p\($0.offset + 1)\" playOrder=\"\($0.offset + 1)\"><navLabel><text>\(escape($0.element.title))</text></navLabel>"
                + "<content src=\"\(chapterPath($0.offset + 1))\"/></navPoint>\n"
        }.joined()
        return """
            <?xml version="1.0" encoding="utf-8"?>
            <ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
            <head><meta name="dtb:uid" content="\(escape(book.identifier))"/></head>
            <docTitle><text>\(escape(book.title))</text></docTitle>
            <navMap>
            \(points)</navMap>
            </ncx>

            """
    }

    private static func document(_ book: Metadata, _ chapter: ChapterDraft) -> String {
        let body = chapter.paragraphs.map {
            $0.heading ? "<h2>\(escape($0.text))</h2>\n" : "<p>\(escape($0.text))</p>\n"
        }.joined()
        return opening(book.language, title: chapter.title) + """
            <link rel="stylesheet" type="text/css" href="../style.css"/></head>
            <body>
            \(body)</body>
            </html>

            """
    }
}
