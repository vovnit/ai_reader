import Foundation

/// Reads a FictionBook 2 file, the format most Russian e-books come in: the
/// title, first author and language from its <title-info>, a chapter from
/// each section of its first <body>, and one last chapter from each later
/// body (notes, comments). Books are matched across devices by title and
/// author and reading places by chapter, so this is the same, word for
/// word, as the Kindle app's `Fb2Book` and the web app's `Fb2Book.js`.
enum FB2Book {
    /// The book in `text`, already decoded. `fallbackTitle` names a book
    /// whose description gives no title.
    static func read(_ text: String, fallbackTitle: String) -> BookDraft {
        let reader = Reader()
        MarkupScanner.scan(text, .init(
            onStart: { name, _ in reader.start(name) },
            onEnd: { reader.end($0) },
            onText: { reader.text($0) }
        ))
        return reader.book(fallbackTitle: fallbackTitle)
    }

    // Each is one paragraph: a <p> in a <title> is a heading, as is a
    // <subtitle>; a table row is its cells joined.
    private static let paragraphElements: Set = ["p", "v", "subtitle", "text-author", "date", "tr"]
    // Each ends the paragraph before it, as the start or end of a paragraph does.
    private static let blocks: Set = [
        "section", "title", "epigraph", "poem", "stanza", "cite", "annotation", "table",
        "body", "empty-line", "image", "binary", "stylesheet",
    ]
    // Never hold anything, so they are not kept open even when written without "/>".
    private static let empty: Set = ["empty-line", "image"]
    private static let nameParts = ["first-name", "middle-name", "last-name"]

    /// Every run of ASCII whitespace made one space, none at the ends; a
    /// no-break space stays, as `.whitespaces` would not leave it.
    private static func collapsed(_ text: String) -> String {
        var out = String.UnicodeScalarView()
        var space = false
        for scalar in text.unicodeScalars {
            if scalar == " " || scalar == "\t" || scalar == "\r" || scalar == "\n" {
                space = !out.isEmpty
                continue
            }
            if space { out.append(" ") }
            space = false
            out.append(scalar)
        }
        return String(out)
    }

    /// The paragraph being read, as cells: one, or a table row's.
    private struct Paragraph {
        var cells = [""]
        var heading: Bool
        var row: Bool

        mutating func add(_ run: String) {
            cells[cells.count - 1] += run
        }
    }

    private final class Reader {
        // The open elements, and how many of each: an end tag closes
        // everything opened after its element, and one that matches nothing
        // is ignored.
        private var stack: [String] = []
        private var counts: [String: Int] = [:]

        // The <title-info> fields, each its first element's text.
        private var fields: [String: String] = [:]
        private var field: String?
        private var authors = 0

        private var bodies = 0
        private var chapters: [ChapterDraft] = []
        private var notes: [ChapterDraft] = []
        private var paragraph: Paragraph?
        // The lines of the title naming a chapter: a section's in the first
        // body, the body's own in a later one.
        private var titleLines: [String]?

        private func inside(_ name: String) -> Bool { counts[name, default: 0] > 0 }
        private var reading: Bool { inside("body") && !inside("binary") && !inside("stylesheet") }

        private func flush() {
            guard let open = paragraph else { return }
            paragraph = nil
            let line = open.cells.map(collapsed).filter { !$0.isEmpty }.joined(separator: " | ")
            if line.isEmpty { return }
            titleLines?.append(line)
            let kept = ChapterDraft.Paragraph(text: line, heading: open.heading)
            // Appended in place: a chapter can hold thousands of paragraphs.
            if bodies > 1 {
                notes[notes.count - 1].paragraphs.append(kept)
            } else {
                chapters[chapters.count - 1].paragraphs.append(kept)
            }
        }

        private func begin(heading: Bool, row: Bool) {
            flush()
            paragraph = Paragraph(heading: heading, row: row)
        }

        func start(_ name: String) {
            if inside("title-info"), !inside("body") {
                if name == "author" { authors += 1 }
                let namePart = name == "nickname" || nameParts.contains(name)
                let wanted = namePart ? inside("author") && authors == 1 : name == "book-title" || name == "lang"
                if wanted, fields[name] == nil {
                    fields[name] = ""
                    field = name
                }
            }
            if blocks.contains(name) { flush() }
            if name == "body" {
                bodies += 1
                if bodies == 1 { chapters.append(ChapterDraft(title: "")) } else { notes.append(ChapterDraft(title: "Notes")) }
            } else if reading {
                if name == "section" {
                    // A section opens a chapter, unless the one before has nothing
                    // but headings yet: then "Part One" and "Chapter 1" open one
                    // together.
                    if bodies == 1, chapters[chapters.count - 1].paragraphs.contains(where: { !$0.heading }) {
                        chapters.append(ChapterDraft(title: ""))
                    }
                } else if name == "title" {
                    if titleLines == nil, stack.last == (bodies == 1 ? "section" : "body") { titleLines = [] }
                } else if paragraphElements.contains(name) {
                    begin(heading: inside("title") || name == "subtitle", row: name == "tr")
                } else if name == "td" || name == "th" {
                    if paragraph?.row != true { begin(heading: inside("title"), row: true) }
                    paragraph?.cells.append("")
                }
            }
            if empty.contains(name) { return }
            stack.append(name)
            counts[name, default: 0] += 1
        }

        private func close(_ name: String) {
            if name == field { field = nil }
            if blocks.contains(name) || paragraphElements.contains(name) { flush() }
            if name == "title", let lines = titleLines {
                if !lines.isEmpty {
                    let title = lines.joined(separator: " ")
                    if bodies > 1 { notes[notes.count - 1].title = title } else { chapters[chapters.count - 1].title = title }
                }
                titleLines = nil
            }
        }

        func end(_ name: String) {
            guard inside(name) else { return }
            while let closed = stack.popLast() {
                counts[closed, default: 0] -= 1
                close(closed)
                if closed == name { return }
            }
        }

        func text(_ run: String) {
            if reading {
                // Text outside any paragraph still reads, as one of its own.
                if paragraph == nil {
                    if collapsed(run).isEmpty { return }
                    paragraph = Paragraph(heading: inside("title"), row: false)
                }
                paragraph?.add(run)
            } else if let field {
                fields[field, default: ""] += run
            }
        }

        func book(fallbackTitle: String) -> BookDraft {
            // A file cut short reads as if everything open closed there.
            while let last = stack.last { end(last) }
            func part(_ name: String) -> String { collapsed(fields[name] ?? "") }
            let bookTitle = part("book-title")
            let title = bookTitle.isEmpty ? fallbackTitle : bookTitle
            let names = nameParts.map(part).filter { !$0.isEmpty }
            let author = names.isEmpty ? part("nickname") : names.joined(separator: " ")
            let language = part("lang").lowercased()
            let kept = (chapters + notes).filter { !$0.paragraphs.isEmpty }.map {
                ChapterDraft(title: $0.title.isEmpty ? title : $0.title, paragraphs: $0.paragraphs)
            }
            return BookDraft(title: title, author: author.isEmpty ? nil : author, language: language.isEmpty ? nil : language, chapters: kept)
        }
    }
}
