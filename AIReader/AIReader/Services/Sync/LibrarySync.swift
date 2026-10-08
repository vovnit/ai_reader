import Foundation
import SQLiteData

/// The books themselves, shared as EPUB files in the `Books` folder beside
/// the sync file. A sync lists the folder, so the library can show what is
/// there, and sends a book here that has no file there yet. A file is
/// fetched only when asked for. The browser extension saves web pages into
/// the same folder.
///
/// A book here without a file is matched by the name it would be given, so
/// a book added on two devices separately ends up on the server once.
enum LibrarySync {
    struct Outcome: Equatable, Sendable {
        var sent = 0
    }

    enum Problem: LocalizedError {
        case gone
        case onShelf(title: String)

        var errorDescription: String? {
            switch self {
            case .gone: "It is no longer in the sync folder."
            case let .onShelf(title): "“\(title)” is already on the shelf."
            }
        }
    }

    static func run(settings: SyncSettings, database: any DatabaseWriter) async throws -> Outcome {
        guard let folder = settings.booksURL else { return Outcome() }
        var outcome = Outcome()

        let remote = try await list(folder, settings: settings)
        try await database.write { db in
            try RemoteBook.delete().execute(db)
            for name in remote { try remember(name, in: db) }
        }
        // Compared without case, since some servers ignore it.
        let byLowercase = Dictionary(remote.map { ($0.lowercased(), $0) }) { first, _ in first }
        var taken = Set(remote)
        let unsent = try await database.read { db in try Book.where { $0.remoteName.is(nil) }.fetchAll(db) }
        for book in unsent {
            let own = RemoteBookName.make(title: book.title, author: book.author, avoiding: [])
            if let same = byLowercase[own.lowercased()] {
                try await database.write { db in try assign(same, to: book.id, in: db) }
                continue
            }
            let name = RemoteBookName.make(title: book.title, author: book.author, avoiding: taken)
            let data = try EPUBPacker.pack(book.directory)
            try await WebDAV.upload(
                data,
                to: folder.appending(path: name),
                type: "application/epub+zip",
                settings: settings
            )
            taken.insert(name)
            try await database.write { db in
                try assign(name, to: book.id, in: db)
                try remember(name, in: db)
            }
            outcome.sent += 1
        }
        return outcome
    }

    /// Fetches one file and shelves it. A book already here that has no
    /// file yet takes this one as its own instead.
    static func fetch(_ name: String, settings: SyncSettings, database: any DatabaseWriter) async throws {
        guard let folder = settings.booksURL else { return }
        guard let data = try await WebDAV.download(folder.appending(path: name), settings: settings) else {
            try await forget(name, database: database)
            throw Problem.gone
        }
        // Named as it was on the server, since a book without a title takes
        // its file's name.
        let file = FileManager.default.temporaryDirectory.appending(path: name)
        try data.write(to: file)
        defer { try? FileManager.default.removeItem(at: file) }
        let unpacked = try EPUBImporter.unpack(epubAt: file)

        let key = BookKey.make(title: unpacked.title, author: unpacked.author)
        let existing = try await database.read { db in try Book.all.fetchAll(db).first { $0.key == key } }
        guard let existing else {
            try await database.write { db in
                let remoteName: String? = name
                try Book.insert {
                    Book.Draft(
                        title: unpacked.title,
                        author: unpacked.author,
                        language: unpacked.language,
                        folder: unpacked.folder,
                        packagePath: unpacked.packagePath,
                        coverPath: unpacked.coverPath,
                        remoteName: remoteName
                    )
                }
                .execute(db)
            }
            return
        }
        try? FileManager.default.removeItem(at: BookStorage.directory(named: unpacked.folder))
        guard existing.remoteName == nil else { throw Problem.onShelf(title: existing.title) }
        try await database.write { db in try assign(name, to: existing.id, in: db) }
    }

    /// Deletes one file from the folder. Devices that have the book keep
    /// their copy.
    static func delete(_ name: String, settings: SyncSettings, database: any DatabaseWriter) async throws {
        guard let folder = settings.booksURL else { return }
        try await WebDAV.delete(folder.appending(path: name), settings: settings)
        try await forget(name, database: database)
    }

    /// The EPUB files in the folder; none when there is no folder yet.
    private static func list(_ folder: URL, settings: SyncSettings) async throws -> [String] {
        do {
            return try await WebDAV.list(folder, settings: settings)
                .filter { !$0.isFolder && RemoteBookName.isBook($0.name) }
                .map(\.name)
        } catch WebDAV.DAVError.http(status: 404) {
            return []
        }
    }

    private static func assign(_ name: String, to bookID: Book.ID, in db: Database) throws {
        let remoteName: String? = name
        try Book.update { $0.remoteName = #bind(remoteName) }.where { $0.id.eq(bookID) }.execute(db)
    }

    private static func remember(_ name: String, in db: Database) throws {
        try RemoteBook.insert { RemoteBook(name: name) }.execute(db)
    }

    private static func forget(_ name: String, database: any DatabaseWriter) async throws {
        try await database.write { db in
            try RemoteBook.where { $0.name.eq(name) }.delete().execute(db)
        }
    }
}
