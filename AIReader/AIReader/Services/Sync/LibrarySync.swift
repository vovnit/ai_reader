import Foundation
import SQLiteData

/// The books themselves, shared as EPUB files in the `Books` folder beside
/// the sync file. A file there that this device has not met is fetched and
/// shelved; a book here that has no file there yet is sent. The browser
/// extension saves web pages into the same folder.
///
/// Removing a book removes it from this device only: the file stays for the
/// others, and is not fetched again since this device has met it.
///
/// Files are fetched before any are sent, so a book added on two devices
/// separately is recognised by its key and ends up on the server once.
enum LibrarySync {
    struct Outcome: Equatable, Sendable {
        var received = 0
        var sent = 0
    }

    static func run(settings: SyncSettings, database: any DatabaseWriter) async throws -> Outcome {
        guard let folder = settings.booksURL else { return Outcome() }
        var outcome = Outcome()

        let remote = try await list(folder, settings: settings)
        let met = Set(try await database.read { db in try RemoteBook.all.fetchAll(db).map(\.name) })
        for name in remote where !met.contains(name) {
            if try await receive(name, from: folder, settings: settings, database: database) {
                outcome.received += 1
            }
        }

        let unsent = try await database.read { db in try Book.where { $0.remoteName.is(nil) }.fetchAll(db) }
        var taken = Set(remote)
        for book in unsent {
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

    /// Fetches one file and shelves it, unless it is a book this device
    /// already has. True when a book was added.
    private static func receive(
        _ name: String,
        from folder: URL,
        settings: SyncSettings,
        database: any DatabaseWriter
    ) async throws -> Bool {
        guard let data = try await WebDAV.download(folder.appending(path: name), settings: settings) else {
            return false
        }
        // Named as it was on the server, since a book without a title takes
        // its file's name.
        let file = FileManager.default.temporaryDirectory.appending(path: name)
        try data.write(to: file)
        defer { try? FileManager.default.removeItem(at: file) }
        // A file that is not a readable EPUB is met all the same, so it is
        // not fetched again on every sync.
        let unpacked = try? EPUBImporter.unpack(epubAt: file)

        let duplicate = try await database.write { db -> Bool in
            try remember(name, in: db)
            guard let unpacked else { return false }
            let key = BookKey.make(title: unpacked.title, author: unpacked.author)
            if let existing = try Book.all.fetchAll(db).first(where: { $0.key == key }) {
                if existing.remoteName == nil { try assign(name, to: existing.id, in: db) }
                return true
            }
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
            return false
        }
        if let unpacked, duplicate {
            try? FileManager.default.removeItem(at: BookStorage.directory(named: unpacked.folder))
        }
        return unpacked != nil && !duplicate
    }

    private static func assign(_ name: String, to bookID: Book.ID, in db: Database) throws {
        let remoteName: String? = name
        try Book.update { $0.remoteName = #bind(remoteName) }.where { $0.id.eq(bookID) }.execute(db)
    }

    private static func remember(_ name: String, in db: Database) throws {
        try RemoteBook
            .insert { RemoteBook(name: name) } onConflict: { $0.name } doUpdate: { row, excluded in
                row.name = excluded.name
            }
            .execute(db)
    }
}
