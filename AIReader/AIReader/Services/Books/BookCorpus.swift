import Foundation

/// The text of the book being read and of the other books in its group,
/// loaded once, when first searched, and searched from any task.
actor BookCorpus {
    /// In the order they are to be searched: the open book first.
    nonisolated let books: [Book]

    private var documents: [Book.ID: BookDocument] = [:]
    private var failures: [Book.ID: String] = [:]

    init(books: [Book]) {
        self.books = books
    }

    nonisolated var severalBooks: Bool { books.count > 1 }

    /// Hands over the book the reader has already loaded, so it is not read
    /// again.
    func provide(_ document: BookDocument, for bookID: Book.ID) {
        documents[bookID] = document
        failures[bookID] = nil
    }

    /// Hits book by book, in reading order, at most `limit`. With `upTo`,
    /// that book is searched only as far as that point — what the reader has
    /// seen — and the others in full.
    func search(_ query: String, limit: Int, upTo: BookPosition? = nil) async -> [SearchHit] {
        await loadMissing()
        var hits: [SearchHit] = []
        for book in books where hits.count < limit {
            guard let document = documents[book.id] else { continue }
            var range: NSRange?
            if let upTo, upTo.bookID == book.id {
                range = NSRange(location: 0, length: min(upTo.offset, document.text.length))
            }
            let found = BookSearch.find(
                in: document.words.text,
                query: query,
                range: range,
                limit: limit - hits.count
            )
            hits += found.map { hit in
                var hit = hit
                hit.bookID = book.id
                hit.bookTitle = book.title
                hit.chapter = document.chapter(containing: hit.offset)
                return hit
            }
        }
        return hits
    }

    /// Why a book could not be read, if one could not; checked after a search.
    var errors: [String] {
        books.compactMap { failures[$0.id] }
    }

    private func loadMissing() async {
        for book in books where documents[book.id] == nil && failures[book.id] == nil {
            do {
                documents[book.id] = try await BookDocumentLoader.load(
                    folder: book.folder,
                    packagePath: book.packagePath
                )
            } catch {
                failures[book.id] = "\(book.title): \(error.localizedDescription)"
            }
        }
    }
}

/// One corpus is the same as itself and no other, which is all a feature's
/// state needs to know.
extension BookCorpus: Equatable {
    nonisolated static func == (lhs: BookCorpus, rhs: BookCorpus) -> Bool { lhs === rhs }
}

/// What is open in front of the reader, as a lookup, an X-ray or a
/// conversation sees it: the books to search, and how far they have read.
struct ReadingScope: Equatable, Sendable {
    var corpus: BookCorpus?
    var upTo: BookPosition?

    static let none = ReadingScope()
}
