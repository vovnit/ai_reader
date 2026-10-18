import Foundation

/// Reads a plain-text book, already decoded. Text wrapped by hand, with
/// blank lines between paragraphs, is joined back into paragraphs; text
/// without blank lines has one paragraph per line. A short paragraph that
/// is a chapter's number, or names a chapter the way a heading does, opens
/// a chapter. The same as the Kindle app's `TextBook` and the web app's
/// `TextBook.js`.
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

    /// "XIV", "12.", "CHAPTER I.", "Глава 3", "Prologue", "BOOK ONE: 1805": a
    /// number alone, or a chapter word alone, before a number, or in a line
    /// without a lower-case letter. "Part of me wanted to stay." is a
    /// sentence. Kept simple, since a heading one app read differently would
    /// move every chapter after it. Compared scalar by scalar, as the other
    /// apps compare.
    static func opensChapter(_ text: String) -> Bool {
        let scalars = Array(text.unicodeScalars)
        guard scalars.count <= headingLength else { return false }
        if isNumber(scalars.last == "." ? scalars.dropLast() : scalars[...]) { return true }
        // A no-break space, which French sets before ":", parts words too.
        let words = scalars.split(whereSeparator: { $0 == " " || $0 == "\u{A0}" })
        guard let firstWord = words.first else { return false }
        let first = Array(String(String.UnicodeScalarView(firstWord)).lowercased().unicodeScalars)
        if words.count == 1 {
            return chapterWords.contains { word in
                let word = Array(word.unicodeScalars)
                return first == word || first == word + ["."] || first == word + [":"]
            }
        }
        let isWord = chapterWords.contains { Array($0.unicodeScalars) == first }
        return isWord && (startsWithNumber(words[1]) || !scalars.contains { $0.properties.generalCategory == .lowercaseLetter })
    }

    /// A chapter's number: up to three digits, or an upper-case roman numeral.
    private static func isNumber(_ text: ArraySlice<Unicode.Scalar>) -> Bool {
        let digits = (1...3).contains(text.count) && text.allSatisfy { ("0"..."9").contains($0) }
        return digits || isRoman(Array(text))
    }

    /// "I.", "12:", "IV—The Return": a number, then nothing or a stop, colon
    /// or dash and anything.
    private static func startsWithNumber(_ word: ArraySlice<Unicode.Scalar>) -> Bool {
        isNumber(word.prefix { !".:\u{2014}-".unicodeScalars.contains($0) })
    }

    /// "I" to "MMMCMXCIX", written the standard way: "DID" and "CIVIL" are
    /// words, not numbers.
    private static func isRoman(_ text: [Unicode.Scalar]) -> Bool {
        var i = 0
        while i < 3, i < text.count, text[i] == "M" { i += 1 }
        i += romanPlace(text, i, one: "C", five: "D", ten: "M")
        i += romanPlace(text, i, one: "X", five: "L", ten: "C")
        i += romanPlace(text, i, one: "I", five: "V", ten: "X")
        return !text.isEmpty && i == text.count
    }

    /// How many letters of `text`, from `i`, one place of a roman numeral
    /// takes when written the standard way: up to three `one`s, `one` and
    /// `five`, `five` and up to three `one`s, or `one` and `ten`.
    private static func romanPlace(_ text: [Unicode.Scalar], _ i: Int, one: Unicode.Scalar, five: Unicode.Scalar, ten: Unicode.Scalar) -> Int {
        func at(_ j: Int) -> Unicode.Scalar? { j < text.count ? text[j] : nil }
        if at(i) == one, at(i + 1) == five || at(i + 1) == ten { return 2 }
        let start = at(i) == five ? 1 : 0
        var ones = 0
        while ones < 3, at(i + start + ones) == one { ones += 1 }
        return start + ones
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
