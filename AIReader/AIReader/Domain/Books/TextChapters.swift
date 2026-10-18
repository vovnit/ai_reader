import Foundation

/// Makes the paragraphs of a plain-text or HTML book into its chapters: each
/// run of paragraphs that open a chapter starts one, titled by the run's
/// last two joined with " — ", and the paragraphs before the first make one
/// titled with the book's title. A book where nothing opens a chapter is cut
/// every `paragraphsPerPart` paragraphs. Chapters with no paragraphs are
/// left out. The same as the Kindle app's `TextChapters` and the web app's
/// `TextChapters.js`.
///
/// Books are matched across devices by title and author, and reading places
/// by chapter number and text, so the three apps must read a file to the
/// same strings: hence one whitespace rule, here, for both readers, and
/// comparisons by Unicode scalar, as JavaScript and C++ make them, rather
/// than by Swift's canonical equivalence.
enum TextChapters {
    /// How many paragraphs make a chapter when nothing in the book opens one.
    static let paragraphsPerPart = 100

    struct Paragraph: Equatable, Sendable {
        var text: String
        var heading = false
        var opens = false
    }

    /// Runs of space, tab, CR and LF made one space and trimmed; a no-break
    /// space, which French sets before ":", stays. (`.whitespaces` would take
    /// it too.)
    static func collapsed(_ text: String) -> String {
        var out = String.UnicodeScalarView()
        var space = false
        for scalar in text.unicodeScalars {
            if isSpace(scalar) {
                space = !out.isEmpty
                continue
            }
            if space { out.append(" ") }
            space = false
            out.append(scalar)
        }
        return String(out)
    }

    /// True when nothing but those spaces and no-break spaces is there: an
    /// `&nbsp;` keeps a paragraph open in HTML but says nothing.
    static func isBlank(_ text: String) -> Bool {
        text.unicodeScalars.allSatisfy { isSpace($0) || $0 == "\u{A0}" }
    }

    static func chapters(_ paragraphs: [Paragraph], title: String) -> [ChapterDraft] {
        var chapters: [ChapterDraft] = []
        if paragraphs.contains(where: \.opens) {
            chapters.append(ChapterDraft(title: title))
            // The run of headings being read. Its last two name the chapter: a
            // part's heading and its first chapter's ("BOOK ONE: 1805 —
            // CHAPTER I"), so the contents still show where a part starts when
            // its chapters restart at I. Any before them are a list of
            // contents running into the book.
            var run: [String] = []
            for (i, paragraph) in paragraphs.enumerated() {
                if paragraph.opens {
                    if i == 0 || !paragraphs[i - 1].opens {
                        if !chapters[chapters.count - 1].paragraphs.isEmpty { chapters.append(ChapterDraft(title: "")) }
                        run = []
                    }
                    run.append(paragraph.text)
                    chapters[chapters.count - 1].title = run.suffix(2).joined(separator: " — ")
                }
                chapters[chapters.count - 1].paragraphs.append(bare(paragraph))
            }
            return chapters
        }
        for start in stride(from: 0, to: paragraphs.count, by: paragraphsPerPart) {
            let part = paragraphs[start..<min(paragraphs.count, start + paragraphsPerPart)]
            chapters.append(ChapterDraft(
                title: paragraphs.count <= paragraphsPerPart ? title : "Part \(start / paragraphsPerPart + 1)",
                paragraphs: part.map(bare)
            ))
        }
        return chapters
    }

    private static func isSpace(_ scalar: Unicode.Scalar) -> Bool {
        scalar == " " || scalar == "\t" || scalar == "\r" || scalar == "\n"
    }

    private static func bare(_ paragraph: Paragraph) -> ChapterDraft.Paragraph {
        ChapterDraft.Paragraph(text: paragraph.text, heading: paragraph.heading)
    }
}
