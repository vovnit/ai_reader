import Foundation
import NaturalLanguage

/// Tokenizes a book once so a tap position can be turned into the word that was
/// touched together with the sentence it sits in.
struct WordContext: @unchecked Sendable {
    struct Selection: Equatable, Sendable {
        let word: String
        let sentence: String
    }

    let text: String
    private let words: [Range<String.Index>]
    private let sentences: [Range<String.Index>]

    init(text: String) {
        self.text = text
        self.words = Self.tokens(in: text, unit: .word)
        self.sentences = Self.tokens(in: text, unit: .sentence)
    }

    /// `offset` is a UTF-16 offset, which is what text views report.
    func selection(atUTF16Offset offset: Int) -> Selection? {
        guard offset >= 0, offset <= text.utf16.count else { return nil }
        let position = String.Index(utf16Offset: offset, in: text)
        guard let word = words.first(where: { $0.contains(position) }) else { return nil }
        return Selection(word: String(text[word]), sentence: sentence(containing: word))
    }

    /// A whitespace-separated piece of the text and the position of its first
    /// letter, so a run of laid-out pieces can report which word was tapped.
    struct Chunk: Equatable, Identifiable, Sendable {
        /// The UTF-16 offset the chunk starts at, unique within the text.
        let id: Int
        let text: String
        let utf16Offset: Int
    }

    var chunks: [Chunk] {
        var chunks: [Chunk] = []
        var index = text.startIndex
        while index < text.endIndex {
            guard !text[index].isWhitespace else { index = text.index(after: index); continue }
            let end = text[index...].firstIndex(where: \.isWhitespace) ?? text.endIndex
            let chunk = text[index..<end]
            let letter = chunk.firstIndex(where: { $0.isLetter || $0.isNumber }) ?? index
            chunks.append(Chunk(
                id: index.utf16Offset(in: text),
                text: String(chunk),
                utf16Offset: letter.utf16Offset(in: text)
            ))
            index = end
        }
        return chunks
    }

    /// The one word in the text, when that is all there is — as with a single
    /// word shared from another app.
    var onlyWord: String? {
        words.count == 1 ? String(text[words[0]]) : nil
    }

    /// The sentence — or run of sentences — covering text the reader selected
    /// by hand. Nil when the range falls outside any sentence.
    func sentence(coveringUTF16Range range: NSRange) -> String? {
        guard let bounds = Range(range, in: text), !bounds.isEmpty else { return nil }
        let last = text.index(before: bounds.upperBound)
        guard let first = sentences.firstIndex(where: { $0.contains(bounds.lowerBound) }),
              let end = sentences[first...].firstIndex(where: { $0.contains(last) })
        else { return nil }
        return String(text[sentences[first].lowerBound..<sentences[end].upperBound])
            .trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private func sentence(containing word: Range<String.Index>) -> String {
        guard let range = sentences.first(where: { $0.contains(word.lowerBound) }) else {
            return String(text[word])
        }
        return String(text[range]).trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private static func tokens(in text: String, unit: NLTokenUnit) -> [Range<String.Index>] {
        let tokenizer = NLTokenizer(unit: unit)
        tokenizer.string = text
        var ranges: [Range<String.Index>] = []
        tokenizer.enumerateTokens(in: text.startIndex..<text.endIndex) { range, _ in
            ranges.append(range)
            return true
        }
        return ranges
    }
}
