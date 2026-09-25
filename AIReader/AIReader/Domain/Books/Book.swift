import Foundation
import SQLiteData

/// A book in the library. The EPUB itself stays unpacked on disk; this row only
/// records where it is and how far it has been read.
@Table
struct Book: Identifiable, Equatable, Sendable {
    let id: Int
    var title = ""
    var author: String?
    var language: String?
    /// Folder name under `BookStorage.root`.
    var folder = ""
    /// Path of the package document, relative to `folder`.
    var packagePath = ""
    /// Path of the cover image, relative to `folder`.
    var coverPath: String?
    var addedAt = Date()
    /// UTF-16 offset of the page the reader was last on.
    var readingOffset = 0
    /// The group the book was put in, if any.
    var groupID: BookGroup.ID?
    /// The same place as `readingOffset`, in the form another device can use.
    @Column(as: ReadingPlace?.JSONRepresentation.self)
    var place: ReadingPlace?
    /// True when `place` arrived from another device and `readingOffset` has
    /// not been worked out from it yet; the reader does that when it opens.
    var placeIsPending = false
    /// When the position or the group last changed, for syncing.
    var updatedAt: Date?
    /// Its file in the sync folder's `Books`, once it is there.
    var remoteName: String?
}

/// A file in the sync folder's `Books` that this device has met: fetched,
/// sent, or found to be a book it already had. A book removed here stays
/// met, so it is not fetched again.
@Table("remoteBooks")
struct RemoteBook: Equatable, Sendable {
    var name = ""
}

extension Book {
    var directory: URL { BookStorage.directory(named: folder) }
    var coverURL: URL? { coverPath.map { directory.appending(path: $0) } }
}
