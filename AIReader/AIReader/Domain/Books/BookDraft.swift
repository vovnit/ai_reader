import Foundation

/// A book read from a file of another format — FB2, plain text or HTML —
/// ready to be written as an EPUB (`EPUBBuilder`). A nil author or language
/// means the file did not say.
struct BookDraft: Equatable, Sendable {
    var title: String
    var author: String?
    var language: String?
    var chapters: [ChapterDraft]
}
