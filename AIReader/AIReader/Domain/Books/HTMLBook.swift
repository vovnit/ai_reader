import Foundation

/// Reads an HTML book, already decoded: each block element's text is a
/// paragraph, a heading's a heading, and `<h1>` and `<h2>` open chapters.
/// Tolerant of what real files carry: unclosed paragraphs, stray end tags,
/// upper-case names, no `<html>` or `<body>`. The same as the Kindle app's
/// `HtmlBook` and the web app's `HtmlBook.js`.
enum HTMLBook {
    /// `fallbackTitle` names a book that does not name itself.
    static func read(_ text: String, fallbackTitle: String) -> BookDraft {
        var title = ""
        var firstH1 = ""
        var author = ""
        var language = ""
        var paragraphs: [TextChapters.Paragraph] = []
        var gathered = ""
        var heading = ""
        var inHead = false
        var inTitle = false
        var titleText = ""
        // The hidden element being skipped, and how deep in ones of its name.
        var skipping = ""
        var depth = 0

        func flush() {
            let paragraph = TextChapters.collapsed(gathered)
            gathered = ""
            if TextChapters.isBlank(paragraph) { return }
            if heading == "h1", firstH1.isEmpty { firstH1 = paragraph }
            paragraphs.append(.init(text: paragraph, heading: !heading.isEmpty, opens: heading == "h1" || heading == "h2"))
        }

        MarkupScanner.scan(withoutRawText(text), .init(
            onStart: { name, attributes in
                if !skipping.isEmpty {
                    if name == skipping { depth += 1 }
                    return
                }
                if !headElements.contains(name) { inHead = false }
                if hidden.contains(name) {
                    skipping = name
                    depth = 1
                } else if name == "html" {
                    language = TextChapters.collapsed(attributes["lang"] ?? "").lowercased()
                } else if name == "head" {
                    inHead = true
                } else if name == "title" {
                    inTitle = true
                    titleText = ""
                } else if name == "meta", TextChapters.collapsed(attributes["name"] ?? "").lowercased() == "author", author.isEmpty {
                    author = TextChapters.collapsed(attributes["content"] ?? "")
                } else if name == "br" {
                    // In a heading a break only sets the words on two lines:
                    // "CHAPTER I.<br>Down the Rabbit-Hole" is one title.
                    if heading.isEmpty { flush() } else { gathered += " " }
                } else if blocks.contains(name) {
                    flush()
                    if headings.contains(name) { heading = name }
                }
            },
            onEnd: { name in
                if !skipping.isEmpty {
                    if name == skipping {
                        depth -= 1
                        if depth == 0 { skipping = "" }
                    }
                    return
                }
                if name == "head" {
                    inHead = false
                } else if name == "title", inTitle {
                    inTitle = false
                    if title.isEmpty { title = TextChapters.collapsed(titleText) }
                } else if blocks.contains(name) {
                    flush()
                    if headings.contains(name) { heading = "" }
                }
            },
            onText: { run in
                if !skipping.isEmpty { return }
                if inTitle {
                    titleText += run
                } else if !inHead {
                    gathered += run
                }
            }
        ))
        flush()

        let gutenberg = Gutenberg.parts(paragraphs.map(\.text))
        if let gutenberg { paragraphs = Array(paragraphs[(gutenberg.start + 1)..<gutenberg.end]) }
        let bookTitle = [gutenberg?.title ?? "", title, firstH1].first { !$0.isEmpty } ?? fallbackTitle
        let bookAuthor = [gutenberg?.author ?? "", author].first { !$0.isEmpty }
        return BookDraft(
            title: bookTitle,
            author: bookAuthor,
            language: language.isEmpty ? nil : language,
            chapters: TextChapters.chapters(paragraphs, title: bookTitle)
        )
    }

    // Elements that end the paragraph before them and begin a new one.
    private static let blocks: Set = [
        "p", "div", "li", "dt", "dd", "blockquote", "pre", "td", "th", "caption", "figcaption", "figure",
        "section", "article", "header", "footer", "aside", "address", "main", "center", "hr",
        "h1", "h2", "h3", "h4", "h5", "h6", "tr", "table", "ul", "ol", "dl", "body",
    ]
    // Elements whose text is not the book's.
    private static let hidden: Set = ["script", "style", "noscript", "template", "svg", "math", "nav"]
    private static let headings: Set = ["h1", "h2", "h3", "h4", "h5", "h6"]
    // Elements a <head> holds; any other ends it, closed or not, as in a browser.
    private static let headElements: Set = ["title", "meta", "link", "style", "script", "base", "noscript", "template"]
    // Elements whose content is not markup, so the scanner never sees it.
    private static let rawTextElements = ["script", "style"]

    /// `markup` without what lies inside `<script>` and `<style>`, where a
    /// "<" would read as a tag that swallows the closing one, and the book
    /// after it. Searched as UTF-8 bytes: every mark looked for is ASCII.
    private static func withoutRawText(_ markup: String) -> String {
        let bytes = Array(markup.utf8)
        let lower = bytes.map { $0 >= 0x41 && $0 <= 0x5A ? $0 + 0x20 : $0 }
        var out: [UInt8] = []
        var kept = 0
        var i = 0
        while let found = find("<", in: lower, from: i) {
            i = found
            if starts(lower, at: i, "<!--") {
                i = find("-->", in: lower, from: i + 4).map { $0 + 3 } ?? lower.count
                continue
            }
            let name = rawTextElements.first { starts(lower, at: i + 1, $0) && endsName(lower, at: i + 1 + $0.utf8.count) }
            guard let name, let open = find(">", in: lower, from: i), lower[open - 1] != UInt8(ascii: "/") else {
                i += 1
                continue
            }
            let close = find("</" + name, in: lower, from: open)
            out += bytes[kept...open]
            kept = close ?? lower.count
            i = kept
        }
        out += bytes[kept...]
        return String(decoding: out, as: UTF8.self)
    }

    private static func endsName(_ bytes: [UInt8], at i: Int) -> Bool {
        guard i < bytes.count else { return false }
        return " \t\r\n/>".utf8.contains(bytes[i])
    }

    private static func starts(_ bytes: [UInt8], at i: Int, _ prefix: String) -> Bool {
        let needle = prefix.utf8
        return i + needle.count <= bytes.count && bytes[i..<(i + needle.count)].elementsEqual(needle)
    }

    private static func find(_ needle: String, in bytes: [UInt8], from start: Int) -> Int? {
        guard let first = needle.utf8.first else { return nil }
        var i = start
        while i < bytes.count, let found = bytes[i...].firstIndex(of: first) {
            if starts(bytes, at: found, needle) { return found }
            i = found + 1
        }
        return nil
    }
}
