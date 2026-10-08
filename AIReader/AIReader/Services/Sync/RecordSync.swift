import Foundation
import SQLiteData

/// One round of syncing the records, after the books (`LibrarySync`): find
/// the files in `aireader-sync` that are due, read them, merge this device's
/// records in and write the result here, send the files the merge changed,
/// and remember where each file stands (`SyncParts`).
enum RecordSync {
    struct Outcome: Equatable, Sendable {
        var applied = SyncStore.Applied()
        var uploaded = false
    }

    struct Unreadable: LocalizedError {
        var name: String
        var errorDescription: String? { "The sync file \(name) on the server could not be read." }
    }

    static func run(settings: SyncSettings, database: any DatabaseWriter) async throws -> Outcome {
        guard let folder = settings.partsURL, let oldFile = settings.oldFileURL else { return Outcome() }
        let key = folder.absoluteString
        let (local, known) = try await database.read { db in
            (try SyncStore.export(db), try knownFiles(in: key, db))
        }
        let fetched = try await fetch(folder: folder, oldFile: oldFile, local: local, known: known, settings: settings)
        let round = try await database.write { db in try reconcile(fetched, in: db) }

        var sent: [String: String] = [:]
        var failure: (any Error)?
        for part in round.outgoing {
            do {
                sent[part.name] = try await WebDAV.upload(
                    part.records.encoded(),
                    to: folder.appending(path: part.name),
                    settings: settings
                )
            } catch {
                failure = error
                break
            }
        }

        // A file that could not be sent is left as it was known, so the next
        // sync takes it up again.
        let unsent = Set(round.outgoing.map(\.name)).subtracting(sent.keys)
        var settled = round.settled.filter { !unsent.contains($0.name) }.map { file in
            var file = file
            if let version = sent[file.name] { file.version = version }
            return file
        }
        // The old file counts as read once everything it brought is on the
        // server.
        if !fetched.oldVersion.isEmpty, unsent.isEmpty {
            settled.append(SyncParts.File(name: SyncParts.oldFile, version: fetched.oldVersion))
        }
        try await database.write { db in
            try remember(SyncParts.remember(try knownFiles(in: key, db), settled: settled), in: key, db)
        }
        if let failure { throw failure }
        return Outcome(applied: round.applied, uploaded: !round.outgoing.isEmpty)
    }

    /// What the server has of the due files.
    private struct Fetched: Sendable {
        /// One part per due file, with no records when the server has none.
        var remote: [SyncParts.Part] = []
        var listed: [SyncParts.File] = []
        /// The old single file's records and version, when it has changed
        /// since this device last read it.
        var old = SyncDocument()
        var oldVersion = ""
    }

    private static func fetch(
        folder: URL,
        oldFile: URL,
        local: SyncDocument,
        known: [SyncParts.File],
        settings: SyncSettings
    ) async throws -> Fetched {
        var fetched = Fetched()
        do {
            fetched.listed = try await WebDAV.list(folder, settings: settings)
                .filter { !$0.isFolder }
                .map { SyncParts.File(name: $0.name, version: $0.version) }
        } catch WebDAV.DAVError.http(status: 404) {}

        let oldKnown = known.first { $0.name == SyncParts.oldFile }?.version
        if let version = try await WebDAV.version(of: oldFile, settings: settings),
           version.isEmpty || version != oldKnown {
            // One that cannot be read is passed over rather than stopping
            // every sync from now on.
            if let data = try await WebDAV.download(oldFile, settings: settings),
               let document = try? SyncDocument.decode(data) {
                fetched.old = document
            }
            fetched.oldVersion = version
        }

        let onServer = Set(fetched.listed.map(\.name))
        for name in SyncParts.due(listed: fetched.listed, known: known, local: local, old: fetched.old) {
            var part = SyncParts.Part(name: name)
            if onServer.contains(name),
               let data = try await WebDAV.download(folder.appending(path: name), settings: settings) {
                guard let records = try? SyncDocument.decode(data) else { throw Unreadable(name: name) }
                part.records = records
            }
            fetched.remote.append(part)
        }
        return fetched
    }

    private struct Round: Sendable {
        /// Due files the merge changed, to be sent.
        var outgoing: [SyncParts.Part] = []
        /// The due files as this device knows them once those are sent.
        var settled: [SyncParts.File] = []
        var applied = SyncStore.Applied()
    }

    /// Merges the due files with this device's records and writes what
    /// changed here — in one transaction, so nothing done meanwhile is lost.
    private static func reconcile(_ fetched: Fetched, in db: Database) throws -> Round {
        var round = Round()
        let merged = SyncParts.merge(fetched.remote, local: try SyncStore.export(db), old: fetched.old)
        round.applied = try SyncStore.apply(SyncParts.join(merged), to: db)

        let versions = Dictionary(fetched.listed.map { ($0.name, $0.version) }) { first, _ in first }
        let digests = Dictionary(
            SyncParts.fingerprints(try SyncStore.export(db)).map { ($0.name, $0.digest) }
        ) { first, _ in first }
        for (part, remote) in zip(merged, fetched.remote) {
            if part.records != remote.records.sorted { round.outgoing.append(part) }
            round.settled.append(
                SyncParts.File(name: part.name, version: versions[part.name] ?? "", digest: digests[part.name] ?? "")
            )
        }
        return round
    }

    private static func knownFiles(in folder: String, _ db: Database) throws -> [SyncParts.File] {
        try SyncFile.where { $0.folder.eq(folder) }.fetchAll(db).map {
            SyncParts.File(name: $0.name, version: $0.version, digest: $0.digest)
        }
    }

    /// Replaces what this device knows of the files, and forgets any other
    /// folder's.
    private static func remember(_ files: [SyncParts.File], in folder: String, _ db: Database) throws {
        try SyncFile.delete().execute(db)
        for file in files {
            try SyncFile.insert {
                SyncFile(folder: folder, name: file.name, version: file.version, digest: file.digest)
            }
            .execute(db)
        }
    }
}
