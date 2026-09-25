import AIReaderCore
import Foundation

/// What two devices agree on: one JSON file holding every book's place and
/// group, and every word looked up with how it has fared in practice. Each
/// record carries when it last changed, and the newer one wins when both
/// devices have it. A word that was deleted stays as a tombstone, so the
/// other device deletes it too instead of bringing it back.
///
/// Times are strings of the form `2026-09-16T10:00:00Z`, so they sort as
/// text and read the same on a Kindle without a date library.
///
/// Reading, writing, ordering and merging are the Kindle app's C++, shared
/// through `Core/`, so the two apps cannot disagree about the file. This type
/// is its Swift face; `SyncDocument+Core.swift` converts between the two.
struct SyncDocument: Equatable, Sendable {
    static let fileName = String(cString: AIReaderCore.SyncDocument.fileName)

    struct BookRecord: Equatable, Sendable {
        var key: String
        var title: String
        var author: String?
        var language: String?
        var group: String?
        var chapter: Int?
        var fraction: Double?
        var snippet: String?
        var updatedAt: String

        var place: ReadingPlace? {
            guard let chapter, let fraction else { return nil }
            return ReadingPlace(chapter: chapter, fraction: fraction, snippet: snippet ?? "")
        }
    }

    struct LookupRecord: Equatable, Sendable {
        var word: String
        var sentence: String
        var lemma = ""
        var formNote = ""
        var meaning = ""
        var language: String?
        /// The key of the book it was read in, if any.
        var book: String?
        var guessed = false
        var confidence = 0.0
        var lookedUpAt = ""
        var correct = 0
        var wrong = 0
        var practicedAt: String?
        var updatedAt: String
        var deleted = false

        var key: String { "\(word)\u{1}\(sentence)" }
    }

    struct Unreadable: Error {}

    var books: [BookRecord] = []
    var lookups: [LookupRecord] = []

    /// Records in a fixed order, so two documents with the same content
    /// compare and encode the same.
    var sorted: SyncDocument {
        SyncDocument(core.sorted())
    }

    /// Both documents' records, the newer of each pair. A tombstone beats a
    /// live record only when it is newer; a lookup made again after being
    /// deleted comes back.
    static func merge(_ local: SyncDocument, _ remote: SyncDocument) -> SyncDocument {
        SyncDocument(AIReaderCore.SyncDocument.merge(local.core, remote.core))
    }

    // MARK: - Reading and writing

    static func decode(_ data: Data) throws -> SyncDocument {
        guard let document = AIReaderCore.SyncDocument.parse(std.string(String(decoding: data, as: UTF8.self))).value
        else { throw Unreadable() }
        return SyncDocument(document)
    }

    func encoded() -> Data {
        Data(String(core.dump()).utf8)
    }

    // MARK: - Times

    private static let formatter: ISO8601DateFormatter = {
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime]
        formatter.timeZone = TimeZone(identifier: "UTC")
        return formatter
    }()

    static func stamp(_ date: Date) -> String {
        formatter.string(from: date)
    }

    static func date(_ stamp: String?) -> Date? {
        stamp.flatMap { formatter.date(from: $0) }
    }
}
