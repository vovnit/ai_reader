import Foundation

/// One place a search query occurs.
struct SearchHit: Equatable, Sendable, Identifiable {
    var bookID: Book.ID = 0
    var bookTitle = ""
    /// The reading-order item the match is in, for the caption.
    var chapter = 0
    /// UTF-16 offset of the match in the book's text.
    var offset = 0
    /// The sentence around the match, cut down when it runs long.
    var excerpt = ""
    /// Where the match sits inside `excerpt`, in UTF-16 units.
    var matchRange = 0..<0

    var id: String { "\(bookID):\(offset)" }

    var position: BookPosition { BookPosition(bookID: bookID, chapter: chapter, offset: offset) }
}

/// A place in a book: a reading-order item and a UTF-16 offset into the
/// book's text.
struct BookPosition: Equatable, Sendable {
    var bookID: Book.ID
    var chapter: Int
    var offset: Int
}

/// Finds a query in a book's text, case-insensitively, and cuts a readable
/// excerpt around every hit. Pure: no display, no disk.
enum BookSearch {
    /// How far an excerpt reaches on either side of its match, in UTF-16
    /// units, when the sentence runs longer.
    static let reach = 220

    /// Hits within `range` of `text`, in order, at most `limit` of them. A
    /// sentence with the query in it twice is one hit.
    static func find(in text: String, query: String, range: NSRange? = nil, limit: Int) -> [SearchHit] {
        let source = text as NSString
        let bounds = range ?? NSRange(location: 0, length: source.length)
        guard !query.isEmpty, limit > 0, bounds.length > 0 else { return [] }

        var hits: [SearchHit] = []
        var from = bounds.location
        // Where the last excerpt ended: a second match in the same sentence
        // is the same passage, listed once.
        var covered = 0
        while hits.count < limit, from < NSMaxRange(bounds) {
            let match = source.range(
                of: query,
                options: .caseInsensitive,
                range: NSRange(location: from, length: NSMaxRange(bounds) - from)
            )
            guard match.location != NSNotFound else { break }
            from = NSMaxRange(match)
            if match.location < covered { continue }
            hits.append(excerpt(in: source, match: match))
            covered = cut(source, match: match, reach: reach).to
        }
        return hits
    }

    /// The excerpt for a match: its sentence, or as much of it as fits within
    /// `reach` on either side.
    static func excerpt(in text: NSString, match: NSRange, reach: Int = reach) -> SearchHit {
        let cut = cut(text, match: match, reach: reach)
        var excerpt = text.substring(with: NSRange(location: cut.from, length: cut.to - cut.from))
        var start = match.location - cut.from
        var end = NSMaxRange(match) - cut.from
        // Trim the whitespace a sentence boundary leaves behind.
        while start > 0, excerpt.first == " " || excerpt.first == "\t" {
            excerpt.removeFirst()
            start -= 1
            end -= 1
        }
        while excerpt.last == " " { excerpt.removeLast() }
        if cut.front {
            excerpt = "…" + excerpt
            start += 1
            end += 1
        }
        if cut.back { excerpt += "…" }
        return SearchHit(offset: match.location, excerpt: excerpt, matchRange: start..<end)
    }

    // MARK: - Sentence boundaries

    /// The stretch of text an excerpt covers, and whether either end was cut.
    private struct Cut {
        var from: Int
        var to: Int
        var front: Bool
        var back: Bool
    }

    private static func cut(_ text: NSString, match: NSRange, reach: Int) -> Cut {
        let start = match.location
        let end = NSMaxRange(match)
        let size = text.length
        var cut = Cut(
            from: sentenceStart(text, at: start, floor: max(0, start - reach)),
            to: sentenceEnd(text, at: end, ceiling: min(size, end + reach)),
            front: false,
            back: false
        )
        cut.front = cut.from > 0 && text.character(at: cut.from - 1) != newline && start - cut.from >= reach
        cut.back = cut.to < size && text.character(at: cut.to) != newline && cut.to - end >= reach
        if cut.front { cut.from = cutForward(text, at: cut.from, ceiling: start) }
        if cut.back { cut.to = cutBackward(text, at: cut.to, floor: end) }
        return cut
    }

    private static let newline: unichar = 0x0A

    private static func endsSentence(_ c: unichar) -> Bool {
        c == 0x2E || c == 0x21 || c == 0x3F || c == 0x2026  // . ! ? …
    }

    private static func isSpace(_ c: unichar) -> Bool {
        UnicodeScalar(c).map { CharacterSet.whitespacesAndNewlines.contains($0) } ?? false
    }

    private static func closesSentence(_ c: unichar) -> Bool {
        c == 0xBB || c == 0x22 || c == 0x29 || c == 0x201D  // » " ) ”
    }

    /// Where the sentence holding `offset` begins: after a paragraph break, or
    /// after the space that follows a full stop.
    private static func sentenceStart(_ text: NSString, at offset: Int, floor: Int) -> Int {
        var p = offset
        while p > floor {
            let c = text.character(at: p - 1)
            if c == newline { return p }
            if isSpace(c), p - 1 > 0, endsSentence(text.character(at: p - 2)) { return p }
            p -= 1
        }
        return p
    }

    /// One past where the sentence holding `offset` ends: at a paragraph
    /// break, or a full stop followed by space or the end of the text.
    private static func sentenceEnd(_ text: NSString, at offset: Int, ceiling: Int) -> Int {
        var p = offset
        while p < ceiling {
            let c = text.character(at: p)
            if c == newline { return p }
            if endsSentence(c) {
                var next = p + 1
                // Closing quotes and brackets belong to the sentence.
                while next < ceiling, closesSentence(text.character(at: next)) { next += 1 }
                if next >= ceiling || isSpace(text.character(at: next)) { return next }
            }
            p += 1
        }
        return ceiling
    }

    /// Moves a cut point to the nearest space on the far side, so a cut never
    /// splits a word or a surrogate pair.
    private static func cutBackward(_ text: NSString, at: Int, floor: Int) -> Int {
        var p = at
        while p > floor, UTF16.isTrailSurrogate(text.character(at: p)) { p -= 1 }
        while p > floor, !isSpace(text.character(at: p)) { p -= 1 }
        return p
    }

    private static func cutForward(_ text: NSString, at: Int, ceiling: Int) -> Int {
        var p = at
        while p < ceiling, UTF16.isTrailSurrogate(text.character(at: p)) { p += 1 }
        while p < ceiling, !isSpace(text.character(at: p)) { p += 1 }
        return p
    }
}
