import Foundation
import NaturalLanguage

/// One distinct word form of a book: what its glossary is written from.
struct BookWord: Equatable, Sendable {
    /// As the dictionaries look it up.
    var form: String
    /// As the book first writes it, capital and all, which tells a name apart.
    var spelling: String
    /// The first places it appears, a few words either side.
    var examples: [String] = []
}

/// Every distinct word form in a book, in reading order, with the first places
/// it appears. Words are the ones a tap finds (`WordContext`), normalized as
/// the dictionaries look them up. Only the first places, so a definition of a
/// name cannot give away what happens later.
enum BookWords {
    private static let examplesPerForm = 3
    /// Words of context on each side of an example.
    private static let contextWords = 8
    private static let joiners = CharacterSet(charactersIn: "'’ʼ-")

    /// `chapters` are the chapters' texts, a paragraph a line.
    static func collect(_ chapters: [String]) -> [BookWord] {
        let tokenizer = NLTokenizer(unit: .word)
        var words: [BookWord] = []
        var indexOf: [String: Int] = [:]
        for chapter in chapters {
            for line in chapter.split(whereSeparator: \.isNewline) {
                let paragraph = String(line)
                tokenizer.string = paragraph
                let found = tokenizer.tokens(for: paragraph.startIndex..<paragraph.endIndex)
                for (index, range) in found.enumerated() {
                    let spelling = String(paragraph[range])
                    guard isDefinable(spelling) else { continue }
                    let form = WordNormalizer.normalize(spelling)
                    guard !form.isEmpty else { continue }
                    let position: Int
                    if let known = indexOf[form] {
                        position = known
                    } else {
                        position = words.count
                        indexOf[form] = position
                        words.append(BookWord(form: form, spelling: spelling))
                    }
                    guard words[position].examples.count < examplesPerForm else { continue }
                    let place = example(in: paragraph, words: found, at: index)
                    if !words[position].examples.contains(place) { words[position].examples.append(place) }
                }
            }
        }
        return words
    }

    /// Letters, joined by apostrophes or hyphens: numbers have nothing to define.
    private static func isDefinable(_ word: String) -> Bool {
        let scalars = word.unicodeScalars
        return scalars.contains { CharacterSet.letters.contains($0) }
            && scalars.allSatisfy { CharacterSet.letters.contains($0) || CharacterSet.nonBaseCharacters.contains($0) || joiners.contains($0) }
    }

    private static func example(in paragraph: String, words: [Range<String.Index>], at index: Int) -> String {
        let start = index <= contextWords ? paragraph.startIndex : words[index - contextWords].lowerBound
        let last = index + contextWords
        let end = last >= words.count - 1 ? paragraph.endIndex : words[last].upperBound
        let text = paragraph[start..<end].trimmingCharacters(in: .whitespaces)
        return (start > paragraph.startIndex ? "…" : "") + text + (end < paragraph.endIndex ? "…" : "")
    }
}
