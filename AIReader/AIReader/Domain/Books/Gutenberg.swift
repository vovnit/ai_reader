import Foundation

/// A Project Gutenberg file wraps its book in a header, which names it, and
/// the license: the book lies between the line "*** START OF …" and the line
/// "*** END OF …", or "End of the Project Gutenberg EBook of …", which older
/// files put before it. The plain-text reader hands its lines here, the HTML
/// reader its paragraphs, so a book reads the same from either file. The
/// same as the Kindle app's `Gutenberg` and the web app's `Gutenberg.js`.
enum Gutenberg {
    struct Parts: Equatable, Sendable {
        /// Where the two markers are.
        var start: Int
        var end: Int
        /// What the header names the book; empty when it does not.
        var title: String
        var author: String
    }

    /// How far into the file the header's "Title:" and "Author:" lines are
    /// looked for.
    private static let headerLines = 60

    /// Nil when `lines` are not a Gutenberg file's.
    static func parts(_ lines: [String]) -> Parts? {
        var start: Int?
        var end: Int?
        for (i, line) in lines.enumerated() where end == nil {
            let lower = TextChapters.collapsed(line).lowercased()
            if start == nil, isStart(lower) {
                start = i
            } else if start != nil, isEnd(lower) {
                end = i
            }
        }
        guard let start, let end else { return nil }

        func field(_ name: String) -> String {
            for line in lines.prefix(min(start, headerLines)) {
                let text = TextChapters.collapsed(line)
                if starts(text, with: name) { return TextChapters.collapsed(dropping(name.unicodeScalars.count, from: text)) }
            }
            return ""
        }
        return Parts(start: start, end: end, title: field("Title:"), author: field("Author:"))
    }

    private static func isMarker(_ lower: String, _ words: String) -> Bool {
        starts(lower, with: "***") && starts(TextChapters.collapsed(dropping(3, from: lower)), with: words)
    }

    private static func isStart(_ lower: String) -> Bool {
        isMarker(lower, "start of")
    }

    private static func isEnd(_ lower: String) -> Bool {
        isMarker(lower, "end of") || starts(lower, with: "end of the project gutenberg") || starts(lower, with: "end of project gutenberg")
    }

    /// Whether `text` begins with `prefix` scalar by scalar, as JavaScript
    /// and C++ compare; `hasPrefix` would also take a canonically
    /// equivalent spelling.
    private static func starts(_ text: String, with prefix: String) -> Bool {
        text.unicodeScalars.starts(with: prefix.unicodeScalars)
    }

    private static func dropping(_ count: Int, from text: String) -> String {
        String(String.UnicodeScalarView(text.unicodeScalars.dropFirst(count)))
    }
}
