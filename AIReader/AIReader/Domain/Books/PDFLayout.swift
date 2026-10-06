import Foundation

/// A bookmark: a title in the PDF's outline and the page it opens.
struct PDFBookmark: Equatable, Sendable {
    var title: String
    var page: Int
}

/// Makes a PDF's text into a book's chapters: one per bookmark, the pages
/// before the first making one of their own. Without bookmarks, each
/// heading opens a chapter, and without headings either, every
/// `pagesPerChapter` pages do. `title` names a chapter nothing else names;
/// chapters with no text are left out. The same as the Kindle app's
/// `PdfLayout`.
enum PDFLayout {
    /// How many pages make a chapter when the PDF has no bookmarks to say.
    static let pagesPerChapter = 10

    static func chapters(_ pages: [[PDFLine]], bookmarks: [PDFBookmark], title: String) -> [ChapterDraft] {
        let paragraphs = PDFParagraphs.read(pages)
        var chapters: [ChapterDraft] = []
        if bookmarks.isEmpty, paragraphs.filter(\.heading).count >= 2 {
            // No bookmarks, but headings: each run of them opens a chapter.
            chapters.append(ChapterDraft(title: title))
            for (i, paragraph) in paragraphs.enumerated() {
                if paragraph.heading, i == 0 || !paragraphs[i - 1].heading {
                    if chapters[chapters.count - 1].paragraphs.isEmpty {
                        chapters[chapters.count - 1].title = paragraph.text
                    } else {
                        chapters.append(ChapterDraft(title: paragraph.text))
                    }
                }
                chapters[chapters.count - 1].paragraphs.append(.init(text: paragraph.text, heading: paragraph.heading))
            }
            return chapters
        }

        // A chapter from each bookmark's page, or from every few pages.
        var marks: [PDFBookmark] = []
        for mark in bookmarks.enumerated().sorted(by: { ($0.element.page, $0.offset) < ($1.element.page, $1.offset) }).map(\.element)
        where marks.last?.page != mark.page {
            marks.append(mark)
        }
        let count = pages.count
        if marks.isEmpty {
            for start in stride(from: 0, to: count, by: pagesPerChapter) {
                let end = min(count, start + pagesPerChapter)
                marks.append(PDFBookmark(title: count <= pagesPerChapter ? title : "Pages \(start + 1)–\(end)", page: start))
            }
        } else if marks[0].page > 0 {
            marks.insert(PDFBookmark(title: title, page: 0), at: 0)
        }
        chapters = marks.map { ChapterDraft(title: $0.title.isEmpty ? title : $0.title) }
        var chapter = 0
        for paragraph in paragraphs {
            while chapter + 1 < marks.count, marks[chapter + 1].page <= paragraph.page { chapter += 1 }
            chapters[chapter].paragraphs.append(.init(text: paragraph.text, heading: paragraph.heading))
        }
        return chapters.filter { !$0.paragraphs.isEmpty }
    }
}
