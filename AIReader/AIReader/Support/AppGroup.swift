import Foundation

/// The app and its Explain extension are two processes. What both need — the
/// database, added dictionaries, settings, the token — lives in the shared
/// app-group container, and the identifiers that name it are kept here.
enum AppGroup {
    static let identifier = "group.dev.nitochkin.AIReader"

    /// The app's own bundle identifier. Keychain items are filed under it, so
    /// the extension must name it too rather than use its own.
    static let appBundleID = "dev.nitochkin.AIReader"
    static let keychainAccessGroup = "37L7BN93A5.\(appBundleID)"

    /// The shared container; Application Support when there is no entitlement
    /// to grant one, as in previews and tests.
    static let container: URL = {
        FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: identifier)
            ?? FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
    }()

    /// Preferences both processes read.
    static var defaults: UserDefaults { UserDefaults(suiteName: identifier) ?? .standard }

    /// The app's bundle, for resources that ship in it. An extension sits at
    /// `App.app/PlugIns/X.appex`, so from there the app is two levels up.
    static let appBundle: Bundle = {
        let main = Bundle.main
        guard main.bundleURL.pathExtension == "appex" else { return main }
        let app = main.bundleURL.deletingLastPathComponent().deletingLastPathComponent()
        return Bundle(url: app) ?? main
    }()

    /// Earlier builds kept everything in Application Support, where the
    /// extension cannot see it. Move what it needs into the shared container
    /// the first time the app runs with one; an existing library must survive.
    static func adoptLegacyFiles() {
        let files = FileManager.default
        let legacy = files.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        guard legacy != container else { return }
        try? files.createDirectory(at: container, withIntermediateDirectories: true)
        for name in ["SQLiteData.db", "SQLiteData.db-wal", "SQLiteData.db-shm", "Dictionaries"] {
            let from = legacy.appending(path: name)
            let to = container.appending(path: name)
            guard files.fileExists(atPath: from.path), !files.fileExists(atPath: to.path) else { continue }
            try? files.moveItem(at: from, to: to)
        }
    }
}
