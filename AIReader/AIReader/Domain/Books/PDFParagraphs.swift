import Foundation

/// One line of a PDF page's text as it is laid out, in points: where it
/// starts and ends, how far below the top of the page its baseline is, and
/// how large its letters are.
struct PDFLine: Equatable, Sendable {
    var text: String
    var left: Double
    var right: Double
    var y: Double
    var size: Double
}

/// A paragraph of a PDF's text, or a heading, and the page it starts on.
struct PDFParagraph: Equatable, Sendable {
    var text: String
    var heading = false
    var page = 0
}

/// Turns a PDF's lines back into paragraphs, undoing what the page did:
/// running heads and page numbers are dropped, lines are joined — a word
/// hyphenated at a line's end mended — and a paragraph a page break cut is
/// joined again. A line set larger than the text is a heading. The same
/// rules as the Kindle app's `PdfParagraphs`.
enum PDFParagraphs {
    static func read(_ pages: [[PDFLine]]) -> [PDFParagraph] {
        let measure = Measure(pages)
        // A line repeated at the top or foot of three pages or more, and set
        // apart from the text, is a running head.
        var repeats: [String: Int] = [:]
        for page in pages {
            var seen: Set<String> = []
            for edge in edges(page) {
                let key = normalized(page[edge.index].text)
                if !key.isEmpty, seen.insert(key).inserted { repeats[key, default: 0] += 1 }
            }
        }
        func isFurniture(_ line: PDFLine, apart: Double) -> Bool {
            guard !measure.isHeading(line) else { return false }
            let key = normalized(line.text)
            return isPageNumber(line.text)
                || (repeats[key, default: 0] >= 3 && apart > measure.gap * 1.3 && key.split(separator: " ").count <= 8)
        }

        var found: [PDFParagraph] = []
        for (number, all) in pages.enumerated() {
            var dropped: Set<Int> = []
            let outer = edges(all)
            // The top two, then the foot two: each stops at the first line
            // that is the book's own.
            for edge in outer.prefix(2) {
                guard isFurniture(all[edge.index], apart: edge.apart) else { break }
                dropped.insert(edge.index)
            }
            for edge in outer.dropFirst(2) {
                guard isFurniture(all[edge.index], apart: edge.apart) else { break }
                dropped.insert(edge.index)
            }
            let lines = all.indices.filter { !dropped.contains($0) }.map { all[$0] }
            let left = edge(lines, measure, left: true)
            let right = edge(lines, measure, left: false)

            for (i, line) in lines.enumerated() {
                let heading = measure.isHeading(line)
                let starts: Bool
                if i == 0 {
                    // A paragraph the page break cut goes on, unindented, in
                    // lower case — or with any word, when it was long and
                    // unfinished.
                    let open = found.last.flatMap { !$0.heading && !finished($0.text) ? $0 : nil }
                    let indented = line.left - left > measure.size * 0.8
                    if let open, !heading, !indented {
                        starts = !(startsLowercase(line.text) || (open.text.utf8.count >= 100 && startsLetter(line.text)))
                    } else {
                        starts = true
                    }
                } else {
                    let above = lines[i - 1]
                    let gap = line.y - above.y
                    if heading && measure.isHeading(above) {
                        starts = gap > line.size * 2
                    } else {
                        starts = heading != measure.isHeading(above)
                            || gap > measure.gap * 1.6 || gap < -measure.gap * 0.5
                            || line.left - above.left > measure.size * 0.8
                            // Two one-line paragraphs, both indented, the first a whole sentence.
                            || (line.left - left > measure.size * 0.8 && abs(line.left - above.left) <= measure.size * 0.8 && finished(above.text))
                            || (above.right < right - measure.size * 2.5 && finished(above.text))
                    }
                }
                if starts {
                    found.append(PDFParagraph(text: line.text, heading: heading, page: number))
                } else {
                    found[found.count - 1].text = joined(found[found.count - 1].text, line.text)
                }
            }
        }
        // Nothing but whitespace, a no-break space included, is no paragraph.
        return found
            .map { PDFParagraph(text: collapsed($0.text), heading: $0.heading, page: $0.page) }
            .filter { !$0.text.unicodeScalars.allSatisfy(\.properties.isWhitespace) }
    }

    /// The size most of the book's letters are set in, and how far apart its
    /// lines usually are.
    private struct Measure {
        var size = 10.0
        var gap = 12.0

        init(_ pages: [[PDFLine]]) {
            var letters: [Double: Int] = [:]
            var total = 0
            for line in pages.joined() {
                letters[(line.size * 10).rounded() / 10, default: 0] += line.text.utf8.count
                total += line.text.utf8.count
            }
            var counted = 0
            for (size, count) in letters.sorted(by: { $0.key < $1.key }) {
                counted += count
                if counted * 2 >= total {
                    self.size = size
                    break
                }
            }
            var gaps: [Double] = []
            for page in pages where page.count > 1 {
                for i in 1..<page.count {
                    let gap = page[i].y - page[i - 1].y
                    if isBody(page[i]), isBody(page[i - 1]), gap > 0, gap < size * 3 { gaps.append(gap) }
                }
            }
            gaps.sort()
            gap = gaps.isEmpty ? size * 1.2 : gaps[gaps.count / 2]
        }

        func isBody(_ line: PDFLine) -> Bool { abs(line.size - size) < size * 0.15 }
        func isHeading(_ line: PDFLine) -> Bool { line.size > size * 1.2 }
    }

    /// Where the text block starts, or ends: the outermost place at least
    /// two body lines agree on, so one stray line cannot move it; the
    /// outermost line when no two agree.
    private static func edge(_ lines: [PDFLine], _ measure: Measure, left: Bool) -> Double {
        let xs = lines.filter(measure.isBody).map { left ? $0.left : $0.right }
        var counts: [Double: Int] = [:]
        for x in xs { counts[x.rounded(.toNearestOrAwayFromZero), default: 0] += 1 }
        let agreed = counts.filter { $0.value >= 2 }.keys
        if let found = left ? agreed.min() : agreed.max() { return found }
        return (left ? xs.min() : xs.max()) ?? (left ? .infinity : -.infinity)
    }

    /// The lines nearest the top and the foot of a page, outermost first,
    /// each with how far it stands from the next line in: where running
    /// heads and page numbers are. The top two come first.
    private static func edges(_ lines: [PDFLine]) -> [(index: Int, apart: Double)] {
        let order = lines.indices.sorted { lines[$0].y < lines[$1].y || (lines[$0].y == lines[$1].y && $0 < $1) }
        let count = order.count
        var found: [(index: Int, apart: Double)] = []
        for k in 0..<min(2, count) {
            found.append((order[k], k + 1 < count ? lines[order[k + 1]].y - lines[order[k]].y : .infinity))
        }
        var k = 0
        while k < 2, k + 2 < count {
            let at = count - 1 - k
            found.append((order[at], lines[order[at]].y - lines[order[at - 1]].y))
            k += 1
        }
        return found
    }

    /// A line as it is compared with other pages' lines: in lower case, its
    /// numbers and extra spaces gone.
    private static func normalized(_ text: String) -> String {
        var out = ""
        for scalar in text.unicodeScalars {
            if ("0"..."9").contains(scalar) { continue }
            if " \t\n\r\u{0B}\u{0C}".unicodeScalars.contains(scalar) {
                if !out.isEmpty, !out.hasSuffix(" ") { out += " " }
                continue
            }
            out.unicodeScalars.append(("A"..."Z").contains(scalar) ? Unicode.Scalar(scalar.value + 32)! : scalar)
        }
        while out.hasSuffix(" ") { out.removeLast() }
        return out
    }

    /// A page number: digits or a roman numeral, perhaps between dashes.
    private static func isPageNumber(_ text: String) -> Bool {
        let bare = text.filter { !" -–—|.·".contains($0) }
        guard !bare.isEmpty, bare.count <= 7 else { return false }
        return bare.allSatisfy { ("0"..."9").contains($0) } || bare.allSatisfy { "ivxlcdmIVXLCDM".contains($0) }
    }

    /// Whether a paragraph ends as a sentence does, rather than being cut.
    private static func finished(_ text: String) -> Bool {
        text.last.map { ".!?:;\")]…»”".contains($0) } ?? false
    }

    private static func startsLowercase(_ text: String) -> Bool {
        text.unicodeScalars.first?.properties.generalCategory == .lowercaseLetter
    }

    private static func startsLetter(_ text: String) -> Bool {
        switch text.unicodeScalars.first?.properties.generalCategory {
        case .uppercaseLetter, .lowercaseLetter, .titlecaseLetter, .modifierLetter, .otherLetter: true
        default: false
        }
    }

    /// A paragraph with a line added; a word broken with a hyphen at the
    /// line's end is mended.
    private static func joined(_ paragraph: String, _ line: String) -> String {
        for hyphen in ["-", "\u{2010}", "\u{00AD}"] where paragraph.unicodeScalars.last.map(String.init) == hyphen {
            if hyphen == "\u{00AD}" || startsLowercase(line) {
                var mended = paragraph
                mended.unicodeScalars.removeLast()
                return mended + line
            }
        }
        return paragraph + " " + line
    }

    /// The text with single spaces, and without the object replacement
    /// character a picture leaves in some PDFs' text, which the reader keeps
    /// for its own pictures.
    private static func collapsed(_ text: String) -> String {
        var out = ""
        for scalar in text.unicodeScalars where scalar != "\u{FFFC}" {
            if scalar == " ", out.isEmpty || out.hasSuffix(" ") { continue }
            out.unicodeScalars.append(scalar)
        }
        while out.hasSuffix(" ") { out.removeLast() }
        return out
    }
}
