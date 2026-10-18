import Foundation

/// A page's lines run together as the page reads — a photo's, or a PDF
/// page's in the page view — so the sentence around a word can be found
/// across line breaks. Each line is joined the way a PDF's lines are, a word
/// hyphenated at a line's end mended, and stands whole at its start in the
/// text, so the character at column c of line i is at `lineStarts[i] + c`.
/// The same as the web app's `PagePassage`.
struct PagePassage: Equatable, Sendable {
    let text: String
    /// Where each line starts in `text`, in UTF-16 units.
    let lineStarts: [Int]

    init(lines: [String]) {
        var text = ""
        var lineStarts: [Int] = []
        for line in lines {
            // An empty line adds nothing.
            guard !line.isEmpty else {
                lineStarts.append(text.utf16.count)
                continue
            }
            text = text.isEmpty ? line : PDFParagraphs.joined(text, line)
            lineStarts.append(text.utf16.count - line.utf16.count)
        }
        self.text = text
        self.lineStarts = lineStarts
    }
}
