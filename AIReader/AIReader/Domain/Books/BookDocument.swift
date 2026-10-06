import Foundation

/// A line of the book's table of contents, pointed into its text.
struct ContentsEntry: Equatable, Sendable {
    var title: String
    /// 0 for a top-level entry; an entry nested under another is deeper.
    var depth: Int
    var chapter: Int
    /// The UTF-16 offset of the place in the book's text.
    var offset: Int
}

/// A book rendered into one attributed string, plus the tokenized text used to
/// resolve taps into words and sentences.
struct BookDocument: @unchecked Sendable {
    let text: NSAttributedString
    let words: WordContext
    /// Where each chapter sits in `text`, in reading order — the items that
    /// had something to show, numbered the way the Kindle app numbers them.
    let chapters: [NSRange]
    /// The language the book is actually written in. EPUB metadata is often
    /// wrong — plenty of French books declare themselves English — so this is
    /// read from the prose instead.
    let language: String?
    /// The book's own table of contents, or one entry a chapter when it has
    /// none.
    let contents: [ContentsEntry]

    init(text: NSAttributedString, chapters: [NSRange]? = nil, contents: [ContentsEntry] = []) {
        self.text = text
        self.words = WordContext(text: text.string)
        self.chapters = chapters ?? [NSRange(location: 0, length: text.length)]
        self.language = TextLanguage.detect(in: text.string)
        self.contents = contents
    }

    /// The entry of the contents a place falls under: the one nearest before
    /// it, or nil before the first. Entries are read as the book lists them,
    /// which is nearly always in order but need not be.
    func contentsEntry(at offset: Int) -> Int? {
        var found: Int?
        for (index, entry) in contents.enumerated() where entry.offset <= offset {
            if let best = found, entry.offset < contents[best].offset { continue }
            found = index
        }
        return found
    }

    /// The chapter holding a UTF-16 offset of the whole text.
    func chapter(containing offset: Int) -> Int {
        max(0, (chapters.lastIndex { $0.location <= offset } ?? 0))
    }

    /// The plain text of one chapter, for searching and for anchoring a
    /// reading place.
    func chapterText(_ index: Int) -> String {
        guard chapters.indices.contains(index) else { return "" }
        return (text.string as NSString).substring(with: chapters[index])
    }

    static let placeholder = BookDocument(
        text: NSAttributedString(string: "Tap any word to look it up.")
    )
}

extension BookDocument: Equatable {
    static func == (lhs: Self, rhs: Self) -> Bool {
        lhs.text.isEqual(to: rhs.text)
    }
}
