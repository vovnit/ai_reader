import Foundation

/// Puts a word into the shape the dictionary's `normalized_form` column uses.
enum WordNormalizer {
    private static let strippable = CharacterSet.punctuationCharacters
        .union(.symbols)
        .union(.whitespacesAndNewlines)

    static func normalize(_ word: String) -> String {
        var text = word.precomposedStringWithCanonicalMapping
        for quote in ["\u{2019}", "\u{02BC}", "\u{FF07}"] {
            text = text.replacingOccurrences(of: quote, with: "'")
        }
        for dash in ["\u{2010}", "\u{2011}", "\u{2012}", "\u{2013}", "\u{2014}"] {
            text = text.replacingOccurrences(of: dash, with: "-")
        }
        return text
            .lowercased()
            .trimmingCharacters(in: strippable)
    }
}
