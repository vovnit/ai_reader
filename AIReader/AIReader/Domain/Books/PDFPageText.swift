import Foundation

/// A PDF page's text as PDFKit reads it (`PDFPage.string`), run together as
/// the page reads (`PagePassage`), and the way between the two: a character
/// PDFKit finds under a tap is found in the passage, and a word of the
/// passage is found on the page to mark it. Offsets are UTF-16, as PDFKit's.
struct PDFPageText: Equatable, Sendable {
    let passage: PagePassage
    /// Where each line starts in the page's own text, and how long it is.
    private let pageLineStarts: [Int]
    private let lineLengths: [Int]

    init(_ page: String) {
        var lines: [String] = []
        var starts: [Int] = []
        let units = Array(page.utf16)
        var start = 0
        var index = 0
        // A line ends at "\n", "\r\n" or "\r".
        while index < units.count {
            guard units[index] == 10 || units[index] == 13 else {
                index += 1
                continue
            }
            lines.append(String(decoding: units[start..<index], as: UTF16.self))
            starts.append(start)
            index += units[index] == 13 && index + 1 < units.count && units[index + 1] == 10 ? 2 : 1
            start = index
        }
        lines.append(String(decoding: units[start...], as: UTF16.self))
        starts.append(start)
        passage = PagePassage(lines: lines)
        pageLineStarts = starts
        lineLengths = lines.map { $0.utf16.count }
    }

    /// Where the page's character `index` is in the passage; nil at a line break.
    func passageOffset(ofPageIndex index: Int) -> Int? {
        guard let line = pageLineStarts.lastIndex(where: { $0 <= index }) else { return nil }
        let column = index - pageLineStarts[line]
        guard column < lineLengths[line] else { return nil }
        return passage.lineStarts[line] + column
    }

    /// The page's characters a stretch of the passage covers — a word mended
    /// across a line break covers the hyphen and the break too.
    func pageRange(ofPassage range: NSRange) -> NSRange {
        let start = pageIndex(ofPassage: range.location)
        let end = pageIndex(ofPassage: max(NSMaxRange(range) - 1, range.location)) + 1
        return NSRange(location: start, length: max(end - start, 0))
    }

    private func pageIndex(ofPassage offset: Int) -> Int {
        let starts = passage.lineStarts
        let line = starts.indices.last { starts[$0] <= offset && lineLengths[$0] > 0 } ?? 0
        let column = min(max(offset - starts[line], 0), max(lineLengths[line] - 1, 0))
        return pageLineStarts[line] + column
    }
}
