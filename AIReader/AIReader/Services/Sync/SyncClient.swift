import ComposableArchitecture
import Foundation
import SQLiteData

/// One round of syncing: exchange the books, then fetch the document, merge
/// this device's records in, write the result back here and to the server.
struct SyncReport: Equatable, Sendable {
    var files = LibrarySync.Outcome()
    var applied = SyncStore.Applied()
    var uploaded = false

    var summary: String {
        var parts: [String] = []
        if files.received > 0 { parts.append("\(files.received) new book\(files.received == 1 ? "" : "s")") }
        if applied.lookups > 0 { parts.append("\(applied.lookups) word\(applied.lookups == 1 ? "" : "s")") }
        if applied.books > 0 { parts.append("\(applied.books) book update\(applied.books == 1 ? "" : "s")") }
        let received = parts.isEmpty ? "Nothing new here" : "Received " + parts.joined(separator: ", ")
        let sent = files.sent > 0 ? "; sent \(files.sent) book\(files.sent == 1 ? "" : "s")" : ""
        return uploaded || files.sent > 0
            ? "\(received)\(sent)\(uploaded ? "; sent changes." : ".")"
            : "\(received); the server was up to date."
    }
}

@DependencyClient
struct SyncClient: Sendable {
    var isConfigured: @Sendable () -> Bool = { false }
    var sync: @Sendable () async throws -> SyncReport
}

extension SyncClient: DependencyKey {
    enum SyncError: LocalizedError {
        case notConfigured
        case badURL

        var errorDescription: String? {
            switch self {
            case .notConfigured: "Enter a WebDAV folder in Settings first."
            case .badURL: "The WebDAV address is not a valid URL."
            }
        }
    }

    static var liveValue: Self {
        Self(
            isConfigured: {
                @Dependency(\.syncSettingsClient) var syncSettings
                return syncSettings.load().isConfigured
            },
            sync: {
                @Dependency(\.syncSettingsClient) var syncSettings
                @Dependency(\.defaultDatabase) var database
                let settings = syncSettings.load()
                guard settings.isConfigured else { throw SyncError.notConfigured }
                guard let url = settings.fileURL else { throw SyncError.badURL }

                var report = SyncReport()
                // Books first, so the places and groups of any that arrive
                // are applied in this same round.
                report.files = try await LibrarySync.run(settings: settings, database: database)

                let remote = try await WebDAV.download(url, settings: settings)
                    .map { try SyncDocument.decode($0) } ?? SyncDocument()
                let local = try await database.read { db in try SyncStore.export(db) }
                let merged = SyncDocument.merge(local, remote)

                report.applied = try await database.write { db in try SyncStore.apply(merged, to: db) }
                if merged != remote.sorted {
                    try await WebDAV.upload(merged.encoded(), to: url, settings: settings)
                    report.uploaded = true
                }
                return report
            }
        )
    }

    static let testValue = Self()
}

extension DependencyValues {
    var syncClient: SyncClient {
        get { self[SyncClient.self] }
        set { self[SyncClient.self] = newValue }
    }
}
