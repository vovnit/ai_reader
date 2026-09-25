import Foundation

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

    init(text: NSAttributedString, chapters: [NSRange]? = nil) {
        self.text = text
        self.words = WordContext(text: text.string)
        self.chapters = chapters ?? [NSRange(location: 0, length: text.length)]
        self.language = TextLanguage.detect(in: text.string)
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
