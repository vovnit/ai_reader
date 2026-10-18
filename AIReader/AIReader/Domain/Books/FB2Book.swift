import Foundation

/// Reads a FictionBook 2 file, the format most Russian e-books come in: the
/// title, first author and language from its <title-info>, a chapter from
/// each section of its text, the first <body> without a name, and one last
/// chapter from each other body (notes, comments). Books are matched across
/// devices by title and author and reading places by chapter, so this is
/// the same, word for word, as the Kindle app's `Fb2Book` and the web app's
/// `Fb2Book.js`.
enum FB2Book {
    /// The book in `text`, already decoded. `fallbackTitle` names a book
    /// whose description gives no title.
    static func read(_ text: String, fallbackTitle: String) -> BookDraft {
        let reader = Reader()
        MarkupScanner.scan(text, .init(
            onStart: { name, attributes in reader.start(name, attributes: attributes) },
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

    /// A body, read as the text is, a chapter from each section, since which
    /// one is the text is known only at the end: whether it has a name, and
    /// its own title, which names it when it is notes.
    private struct Body {
        var named: Bool
        var title = ""
        var chapters = [ChapterDraft(title: "")]
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

        private var bodies: [Body] = []
        private var paragraph: Paragraph?
        // The lines of the title being read, a body's or a section's.
        private var titleLines: [String]?
        private var titled = ""

        private func inside(_ name: String) -> Bool { counts[name, default: 0] > 0 }
        private var reading: Bool { inside("body") && !inside("binary") && !inside("stylesheet") }

        /// Changes the chapter being read in place: it can hold thousands of
        /// paragraphs.
        private func changeChapter(_ change: (inout ChapterDraft) -> Void) {
            let body = bodies.count - 1
            let chapter = bodies[body].chapters.count - 1
            change(&bodies[body].chapters[chapter])
        }

        private func flush() {
            guard let open = paragraph else { return }
            paragraph = nil
            // A cell of nothing but no-break spaces says nothing, like an empty one.
            let line = open.cells.map(collapsed).filter { !TextChapters.isBlank($0) }.joined(separator: " | ")
            if line.isEmpty { return }
            titleLines?.append(line)
            changeChapter { $0.paragraphs.append(ChapterDraft.Paragraph(text: line, heading: open.heading)) }
        }

        private func begin(heading: Bool, row: Bool) {
            flush()
            paragraph = Paragraph(heading: heading, row: row)
        }

        func start(_ name: String, attributes: [String: String]) {
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
                bodies.append(Body(named: attributes["name"] != nil))
            } else if reading {
                if name == "section" {
                    // A section opens a chapter, unless the one before has nothing
                    // but headings yet: then "Part One" and "Chapter 1" open one
                    // together.
                    if bodies[bodies.count - 1].chapters.last?.paragraphs.contains(where: { !$0.heading }) == true {
                        bodies[bodies.count - 1].chapters.append(ChapterDraft(title: ""))
                    }
                } else if name == "title" {
                    if titleLines == nil, let parent = stack.last, parent == "section" || parent == "body" {
                        titleLines = []
                        titled = parent
                    }
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
                let line = lines.joined(separator: " ")
                titleLines = nil
                // A section joining a part's heading adds its title to the
                // part's: "Part One — Chapter 1". An untitled chapter takes the
                // book's title only at the end, so a section's simply names it.
                if !line.isEmpty, titled == "body" {
                    bodies[bodies.count - 1].title = line
                } else if !line.isEmpty {
                    changeChapter { $0.title = $0.title.isEmpty ? line : $0.title + " — " + line }
                }
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
            // FB2 names its notes and comments bodies. Each body but the text
            // is one last chapter, in file order, titled by its own title or
            // "Notes".
            let main = bodies.firstIndex { !$0.named } ?? bodies.indices.first
            let notes = bodies.indices.filter { $0 != main }.map { index in
                ChapterDraft(title: bodies[index].title.isEmpty ? "Notes" : bodies[index].title, paragraphs: bodies[index].chapters.flatMap(\.paragraphs))
            }
            let kept = ((main.map { bodies[$0].chapters } ?? []) + notes).filter { !$0.paragraphs.isEmpty }.map {
                ChapterDraft(title: $0.title.isEmpty ? title : $0.title, paragraphs: $0.paragraphs)
            }
            return BookDraft(title: title, author: author.isEmpty ? nil : author, language: language.isEmpty ? nil : language, chapters: kept)
        }
    }
}
