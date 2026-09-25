import ComposableArchitecture
import Foundation
import SQLiteData

/// Adding, removing and updating books in the library.
@DependencyClient
struct LibraryClient: Sendable {
    var add: @Sendable (_ epub: URL) async throws -> Void
    var delete: @Sendable (_ book: Book) async throws -> Void
    /// Where the reader is, as an offset for this device and as a place any
    /// device can find again.
    var saveReadingOffset: @Sendable (_ bookID: Int, _ offset: Int, _ place: ReadingPlace?) async throws -> Void
    var saveLanguage: @Sendable (_ bookID: Int, _ language: String) async throws -> Void
    var find: @Sendable (_ bookID: Int) async -> Book? = { _ in nil }
    /// The books of one group, oldest first: the order they were shelved in.
    var inGroup: @Sendable (_ groupID: BookGroup.ID) async -> [Book] = { _ in [] }
    var groupName: @Sendable (_ groupID: BookGroup.ID) async -> String? = { _ in nil }
}

extension LibraryClient: DependencyKey {
    static var liveValue: Self {
        Self(
            add: { url in
                @Dependency(\.defaultDatabase) var database
                let scoped = url.startAccessingSecurityScopedResource()
                defer { if scoped { url.stopAccessingSecurityScopedResource() } }

                let unpacked = try EPUBImporter.unpack(epubAt: url)
                try await database.write { db in
                    try Book.insert {
                        Book.Draft(
                            title: unpacked.title,
                            author: unpacked.author,
                            language: unpacked.language,
                            folder: unpacked.folder,
                            packagePath: unpacked.packagePath,
                            coverPath: unpacked.coverPath
                        )
                    }
                    .execute(db)
                }
            },
            delete: { book in
                @Dependency(\.defaultDatabase) var database
                try await database.write { db in
                    try Book.delete().where { $0.id.eq(book.id) }.execute(db)
                }
                try? FileManager.default.removeItem(at: book.directory)
            },
            saveReadingOffset: { bookID, offset, place in
                @Dependency(\.defaultDatabase) var database
                @Dependency(\.date.now) var now
                try await database.write { db in
                    try Book
                        .update {
                            $0.readingOffset = offset
                            $0.place = #bind(place, as: ReadingPlace?.JSONRepresentation.self)
                            $0.placeIsPending = false
                            $0.updatedAt = #bind(now)
                        }
                        .where { $0.id.eq(bookID) }
                        .execute(db)
                }
            },
            saveLanguage: { bookID, language in
                @Dependency(\.defaultDatabase) var database
                try await database.write { db in
                    try Book
                        .update { $0.language = #bind(language) }
                        .where { $0.id.eq(bookID) }
                        .execute(db)
                    // Past lookups were recorded under the wrong language, and
                    // the words list speaks them; correct those too.
                    try Lookup
                        .update { $0.language = #bind(language) }
                        .where { $0.bookID.eq(bookID) }
                        .execute(db)
                }
            },
            find: { bookID in
                @Dependency(\.defaultDatabase) var database
                return try? await database.read { db in
                    try Book.where { $0.id.eq(bookID) }.fetchOne(db)
                }
            },
            inGroup: { groupID in
                @Dependency(\.defaultDatabase) var database
                return (try? await database.read { db in
                    try Book.where { $0.groupID.eq(groupID) }.order { $0.addedAt }.fetchAll(db)
                }) ?? []
            },
            groupName: { groupID in
                @Dependency(\.defaultDatabase) var database
                return try? await database.read { db in
                    try BookGroup.where { $0.id.eq(groupID) }.fetchOne(db)?.name
                }
            }
        )
    }
}

extension DependencyValues {
    var libraryClient: LibraryClient {
        get { self[LibraryClient.self] }
        set { self[LibraryClient.self] = newValue }
    }
}
