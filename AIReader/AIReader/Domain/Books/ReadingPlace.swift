import Foundation

/// Where a reader is in a book, in terms that mean the same on every device:
/// the chapter, how far into it, and the words at that point. Offsets differ
/// between apps — UTF-16 units here, bytes on the Kindle — and between
/// renderings, so a place is found again by its words, and by its fraction
/// when the words cannot be found.
///
/// The words are compared loosely: every run of whitespace, breaking or not,
/// counts as one space, and illustration placeholders do not count at all,
/// since the two apps render those differently.
struct ReadingPlace: Equatable, Sendable, Codable {
    var chapter: Int
    /// How far into the chapter's text, from 0 to 1.
    var fraction: Double
    /// The text starting at the place, a few words of it, normalized.
    var snippet: String

    /// How much text is kept as the snippet, in UTF-16 units, before it is
    /// cut back to a word boundary.
    static let snippetLength = 80

    /// The place at `offset` (UTF-16) of a chapter's text.
    init(chapter: Int, chapterText: String, offset: Int) {
        let text = chapterText as NSString
        let at = min(max(offset, 0), text.length)
        self.chapter = chapter
        self.fraction = text.length > 0 ? Double(at) / Double(text.length) : 0

        let ahead = text.substring(with: NSRange(location: at, length: min(text.length - at, Self.snippetLength * 3)))
        let normalized = Self.normalize(ahead).text as NSString
        var end = min(normalized.length, Self.snippetLength)
        // Cut back to a space so the snippet is whole words, unless that
        // would leave nothing.
        if end < normalized.length {
            var cut = end
            while cut > 0, normalized.character(at: cut - 1) != Self.space { cut -= 1 }
            if cut > 0 { end = cut }
        }
        self.snippet = normalized.substring(to: end).trimmingCharacters(in: .whitespaces)
    }

    init(chapter: Int, fraction: Double, snippet: String) {
        self.chapter = chapter
        self.fraction = fraction
        self.snippet = snippet
    }

    /// The UTF-16 offset in the chapter's text this place stands for: where
    /// the snippet is found — the nearest of its occurrences to the fraction
    /// — else the fraction alone.
    func resolve(in chapterText: String) -> Int {
        let original = chapterText as NSString
        let guess = Double(original.length) * min(max(fraction, 0), 1)
        guard !snippet.isEmpty else { return Int(guess.rounded(.down)) }

        let (normalized, map) = Self.normalize(chapterText)
        let haystack = normalized as NSString
        let expected = Double(haystack.length) * min(max(fraction, 0), 1)
        var best: Int?
        var from = 0
        while from < haystack.length {
            let found = haystack.range(of: snippet, range: NSRange(location: from, length: haystack.length - from))
            guard found.location != NSNotFound else { break }
            if best.map({ abs(Double(found.location) - expected) < abs(Double($0) - expected) }) ?? true {
                best = found.location
            }
            from = found.location + 1
        }
        guard let best, best < map.count else { return Int(guess.rounded(.down)) }
        return map[best]
    }

    // MARK: - Loose comparison

    private static let space: unichar = 0x20
    private static let placeholder: unichar = 0xFFFC

    /// `text` with whitespace collapsed and placeholders dropped, and for
    /// each unit of the result the offset it came from.
    static func normalize(_ text: String) -> (text: String, map: [Int]) {
        let source = text as NSString
        var units: [unichar] = []
        var map: [Int] = []
        var pendingSpace = false
        for index in 0..<source.length {
            let c = source.character(at: index)
            if c == placeholder { continue }
            if UnicodeScalar(c).map({ CharacterSet.whitespacesAndNewlines.contains($0) }) ?? false {
                pendingSpace = !units.isEmpty
                continue
            }
            if pendingSpace {
                units.append(space)
                map.append(index)
                pendingSpace = false
            }
            units.append(c)
            map.append(index)
        }
        return (String(utf16CodeUnits: units, count: units.count), map)
    }
}
