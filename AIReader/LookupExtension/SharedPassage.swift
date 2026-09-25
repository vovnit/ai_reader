import Foundation

/// What the share sheet handed over, settled into something to explain: the
/// passage to show, and the word itself when the selection was just one.
struct SharedPassage: Equatable, Sendable {
    /// The text the reader taps a word in — the sentence around the selection
    /// when the source could supply it, otherwise the selection itself.
    let text: String
    /// Set when the selection was a single word, so no tap is needed.
    let word: WordContext.Selection?
    let language: String?

    /// `paragraph` is the block the selection was taken from and `offset`
    /// where in it the selection starts, which only Safari can report;
    /// `language` is the page's declared language.
    init(selection: String, paragraph: String? = nil, offset: Int? = nil, language: String? = nil) {
        let trimmed = selection.trimmingCharacters(in: .whitespacesAndNewlines)
        let around = paragraph.flatMap { Self.sentence(around: selection, at: offset, in: $0) }
        let selection = trimmed
        text = around ?? selection
        word = WordContext(text: selection).onlyWord.map {
            WordContext.Selection(word: $0, sentence: around ?? "")
        }
        self.language = language.flatMap { $0.isEmpty ? nil : $0 }
    }

    /// Finds the selection in the paragraph — at the reported offset when it
    /// really is there, otherwise wherever it first occurs — and returns the
    /// sentence around it.
    private static func sentence(around selection: String, at offset: Int?, in paragraph: String) -> String? {
        let trimmed = selection.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return nil }
        let context = WordContext(text: paragraph)
        if let offset, let range = Range(NSRange(location: offset, length: selection.utf16.count), in: paragraph),
           paragraph[range] == selection,
           let found = paragraph.range(of: trimmed, range: range) {
            return context.sentence(coveringUTF16Range: NSRange(found, in: paragraph))
        }
        guard let found = paragraph.range(of: trimmed) else { return nil }
        return context.sentence(coveringUTF16Range: NSRange(found, in: paragraph))
    }
}
