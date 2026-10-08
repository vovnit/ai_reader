import ComposableArchitecture
import Foundation

/// Where sync keeps its files: a WebDAV folder and the account that may
/// write to it. Nothing is synced until a URL is given.
struct SyncSettings: Equatable, Sendable {
    var url = ""
    var username = ""
    var password = ""

    var isConfigured: Bool { !url.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }

    /// The sync folder itself.
    var folderURL: URL? { URL(string: url.trimmingCharacters(in: .whitespacesAndNewlines)) }

    /// The server alone, for reaching other folders on it.
    var serverURL: URL? {
        guard let folder = folderURL, var components = URLComponents(url: folder, resolvingAgainstBaseURL: false)
        else { return nil }
        components.path = "/"
        components.query = nil
        return components.url
    }

    /// The books' folder inside the sync folder.
    var booksURL: URL? {
        folderURL?.appending(path: RemoteBookName.folder, directoryHint: .isDirectory)
    }

    /// The folder of record files inside the sync folder (`SyncParts`).
    var partsURL: URL? {
        folderURL?.appending(path: SyncParts.folder, directoryHint: .isDirectory)
    }

    /// The one file older versions kept the records in.
    var oldFileURL: URL? {
        folderURL?.appending(path: SyncParts.oldFile, directoryHint: .notDirectory)
    }
}

/// Reads and writes the sync settings.
@DependencyClient
struct SyncSettingsClient: Sendable {
    var load: @Sendable () -> SyncSettings = { SyncSettings() }
    var save: @Sendable (_ settings: SyncSettings) -> Void
}

extension SyncSettingsClient: DependencyKey {
    private enum Key {
        static let url = "syncURL"
        static let username = "syncUsername"
        static let password = "syncPassword"
    }

    static let liveValue = Self(
        load: {
            let defaults = UserDefaults.standard
            return SyncSettings(
                url: defaults.string(forKey: Key.url) ?? "",
                username: defaults.string(forKey: Key.username) ?? "",
                password: Keychain.string(forKey: Key.password) ?? ""
            )
        },
        save: { settings in
            let defaults = UserDefaults.standard
            defaults.set(settings.url, forKey: Key.url)
            defaults.set(settings.username, forKey: Key.username)
            // The password is a secret, so it lives in the keychain.
            Keychain.set(settings.password, forKey: Key.password)
        }
    )

    static let testValue = Self()
}

extension DependencyValues {
    var syncSettingsClient: SyncSettingsClient {
        get { self[SyncSettingsClient.self] }
        set { self[SyncSettingsClient.self] = newValue }
    }
}
