import ComposableArchitecture
import Foundation
import SQLiteData

/// One round of syncing: exchange the books, then the records
/// (`RecordSync`).
struct SyncReport: Equatable, Sendable {
    var files = LibrarySync.Outcome()
    var applied = SyncStore.Applied()
    var uploaded = false

    var summary: String {
        var parts: [String] = []
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
    /// Fetches a book from the sync folder and shelves it.
    var download: @Sendable (_ name: String) async throws -> Void
    /// Deletes a book's file from the sync folder.
    var deleteRemote: @Sendable (_ name: String) async throws -> Void
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
        @Sendable func configured() throws -> SyncSettings {
            @Dependency(\.syncSettingsClient) var syncSettings
            let settings = syncSettings.load()
            guard settings.isConfigured else { throw SyncError.notConfigured }
            guard settings.booksURL != nil else { throw SyncError.badURL }
            return settings
        }
        return Self(
            isConfigured: {
                @Dependency(\.syncSettingsClient) var syncSettings
                return syncSettings.load().isConfigured
            },
            sync: {
                @Dependency(\.syncSettingsClient) var syncSettings
                @Dependency(\.defaultDatabase) var database
                let settings = syncSettings.load()
                guard settings.isConfigured else { throw SyncError.notConfigured }
                guard settings.partsURL != nil else { throw SyncError.badURL }

                var report = SyncReport()
                report.files = try await LibrarySync.run(settings: settings, database: database)
                let records = try await RecordSync.run(settings: settings, database: database)
                report.applied = records.applied
                report.uploaded = records.uploaded
                return report
            },
            download: { name in
                @Dependency(\.defaultDatabase) var database
                try await LibrarySync.fetch(name, settings: configured(), database: database)
            },
            deleteRemote: { name in
                @Dependency(\.defaultDatabase) var database
                try await LibrarySync.delete(name, settings: configured(), database: database)
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
