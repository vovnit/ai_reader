import Foundation

/// Reads a plain-text book, already decoded. Text wrapped by hand, with
/// blank lines between paragraphs, is joined back into paragraphs; text
/// without blank lines has one paragraph per line. A short paragraph that
/// names a chapter or is only a number opens a chapter. The same as the
/// Kindle app's `TextBook` and the web app's `TextBook.js`.
enum TextBook {
    /// `fallbackTitle` names a book that does not name itself.
    static func read(_ text: String, fallbackTitle: String) -> BookDraft {
        var lines = splitLines(text)
        let gutenberg = Gutenberg.parts(lines)
        if let gutenberg { lines = Array(lines[(gutenberg.start + 1)..<gutenberg.end]) }

        // Wrapped by hand when a blank line stands between two lines of text.
        var wrapped = false
        if let first = lines.firstIndex(where: { !TextChapters.isBlank($0) }),
           let last = lines.lastIndex(where: { !TextChapters.isBlank($0) }) {
            wrapped = lines[first...last].contains(where: TextChapters.isBlank)
        }

        var paragraphs: [TextChapters.Paragraph] = []
        var run: [String] = []
        func end() {
            guard !run.isEmpty else { return }
            let paragraph = TextChapters.collapsed(run.joined(separator: " "))
            let opens = opensChapter(paragraph)
            paragraphs.append(.init(text: paragraph, heading: opens, opens: opens))
            run = []
        }
        for line in lines {
            if TextChapters.isBlank(line) {
                end()
            } else {
                run.append(line)
                if !wrapped { end() }
            }
        }
        end()

        let title = gutenberg.map(\.title).flatMap { $0.isEmpty ? nil : $0 } ?? fallbackTitle
        let author = gutenberg.map(\.author).flatMap { $0.isEmpty ? nil : $0 }
        return BookDraft(title: title, author: author, language: nil, chapters: TextChapters.chapters(paragraphs, title: title))
    }

    /// "CHAPTER I.", "Глава 3", "XIV", "12.": kept simple, since a heading
    /// one app read differently would move every chapter after it. Compared
    /// scalar by scalar, as the other apps compare.
    static func opensChapter(_ text: String) -> Bool {
        let scalars = text.unicodeScalars
        guard scalars.count <= headingLength else { return false }
        let number = scalars.last == "." ? scalars.dropLast() : scalars[...]
        if !number.isEmpty, number.allSatisfy({ ("0"..."9").contains($0) }) || number.allSatisfy({ "IVXLCDM".unicodeScalars.contains($0) }) {
            return true
        }
        let lower = text.lowercased().unicodeScalars
        return chapterWords.contains { word in
            lower.elementsEqual(word.unicodeScalars) || lower.starts(with: (word + " ").unicodeScalars)
        }
    }

    /// The longest paragraph, in code points, still read as a chapter's heading.
    private static let headingLength = 80

    // Words a chapter heading begins with, lowercased: English, French,
    // Spanish, Italian, German, Russian, Ukrainian.
    private static let chapterWords = [
        "chapter", "part", "book", "prologue", "epilogue",
        "chapitre", "partie", "livre", "épilogue",
        "capítulo", "capitulo", "parte", "libro",
        "kapitel", "teil", "buch",
        "глава", "часть", "книга", "пролог", "эпилог", "розділ",
    ]

    /// The lines, whether they end in LF, CRLF or CR.
    private static func splitLines(_ text: String) -> [String] {
        var lines: [String] = []
        var line = String.UnicodeScalarView()
        var afterCR = false
        for scalar in text.unicodeScalars {
            if scalar == "\n", afterCR {
                afterCR = false
                continue
            }
            afterCR = scalar == "\r"
            if scalar == "\r" || scalar == "\n" {
                lines.append(String(line))
                line = String.UnicodeScalarView()
            } else {
                line.append(scalar)
            }
        }
        lines.append(String(line))
        return lines
    }
}
