import Foundation

/// Where the elements a table of contents links to begin in a chapter's
/// text. The HTML import keeps no ids, so each wanted one is marked in the
/// markup, before its element's first text, with a character from a private
/// plane; after the import the marks are read back and taken out, leaving
/// the text as it would have been.
enum ChapterAnchors {
    /// `markup` with a mark at each of `ids` it has.
    static func mark(_ ids: [String], in markup: String) -> String {
        var marked = markup
        for (index, id) in ids.enumerated() where index < limit {
            let escaped = NSRegularExpression.escapedPattern(for: id)
            // The element with the id, any tags opening inside it, and the
            // space before its text, which the import would drop.
            let pattern = "<[A-Za-z][^>]*\\sid\\s*=\\s*[\"']\(escaped)[\"'][^>]*>(?:\\s*<[A-Za-z][^>]*>)*\\s*"
            guard let found = marked.range(of: pattern, options: .regularExpression) else { continue }
            marked.unicodeScalars.insert(Unicode.Scalar(first + UInt32(index))!, at: found.upperBound)
        }
        return marked
    }

    /// The UTF-16 offset of each mark in `text`, by its place in the ids
    /// given to `mark`, with the marks taken out of `text`. A mark the
    /// import set in a paragraph of its own goes with its line break.
    static func locate(in text: NSMutableAttributedString) -> [Int: Int] {
        let string = text.string as NSString
        var marks: [(index: Int, range: NSRange)] = []
        let scalars = text.string.unicodeScalars
        var position = scalars.startIndex
        while position < scalars.endIndex {
            let next = scalars.index(after: position)
            let value = scalars[position].value
            if value >= first, value < first + UInt32(limit) {
                var range = NSRange(position..<next, in: text.string)
                let alone = (range.location == 0 || string.character(at: range.location - 1) == 0x0A)
                    && NSMaxRange(range) < string.length && string.character(at: NSMaxRange(range)) == 0x0A
                if alone { range.length += 1 }
                marks.append((Int(value - first), range))
            }
            position = next
        }
        var offsets: [Int: Int] = [:]
        var removed = 0
        for mark in marks {
            offsets[mark.index] = mark.range.location - removed
            removed += mark.range.length
        }
        for mark in marks.reversed() { text.deleteCharacters(in: mark.range) }
        return offsets
    }

    /// Plane 15, private use: no book's text has it.
    private static let first: UInt32 = 0xF0000
    private static let limit = 0xFFFD
}
