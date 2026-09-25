import ComposableArchitecture
import Foundation

/// Which player to follow and how to reach it. VLC answers on the Mac it
/// runs on; Kodi is a box on the network, found by Bonjour or given by
/// address. Both are guarded by a password.
struct PlayerSettings: Equatable, Sendable {
    enum Kind: String, CaseIterable, Sendable {
        case vlc, kodi

        var title: String { self == .vlc ? "VLC" : "Kodi" }
    }

    #if os(macOS)
    var kind: Kind = .vlc
    #else
    var kind: Kind = .kodi
    #endif
    var host = ""
    var port = "8080"
    var username = ""
    var password = ""

    /// The player's web server; VLC's is on this machine, Kodi's wherever the
    /// TV is.
    var baseURL: URL? {
        var components = URLComponents()
        components.scheme = "http"
        components.host = host.isEmpty && kind == .vlc ? "localhost" : host.trimmingCharacters(in: .whitespaces)
        components.port = Int(port.trimmingCharacters(in: .whitespaces))
        guard let host = components.host, !host.isEmpty else { return nil }
        return components.url
    }

    /// VLC takes any user name; Kodi's is `kodi` unless changed.
    var authorization: String {
        let user = username.isEmpty && kind == .kodi ? "kodi" : username
        return "Basic " + Data("\(user):\(password)".utf8).base64EncodedString()
    }
}

/// Reads and writes the player settings.
@DependencyClient
struct PlayerSettingsClient: Sendable {
    var load: @Sendable () -> PlayerSettings = { PlayerSettings() }
    var save: @Sendable (_ settings: PlayerSettings) -> Void
}

extension PlayerSettingsClient: DependencyKey {
    private enum Key {
        static let kind = "playerKind"
        static let host = "playerHost"
        static let port = "playerPort"
        static let username = "playerUsername"
        static let password = "playerPassword"
    }

    static let liveValue = Self(
        load: {
            let defaults = UserDefaults.standard
            var settings = PlayerSettings()
            if let kind = defaults.string(forKey: Key.kind).flatMap(PlayerSettings.Kind.init(rawValue:)) {
                settings.kind = kind
            }
            settings.host = defaults.string(forKey: Key.host) ?? ""
            settings.port = defaults.string(forKey: Key.port) ?? settings.port
            settings.username = defaults.string(forKey: Key.username) ?? ""
            settings.password = Keychain.string(forKey: Key.password) ?? ""
            return settings
        },
        save: { settings in
            let defaults = UserDefaults.standard
            defaults.set(settings.kind.rawValue, forKey: Key.kind)
            defaults.set(settings.host, forKey: Key.host)
            defaults.set(settings.port, forKey: Key.port)
            defaults.set(settings.username, forKey: Key.username)
            // The password is a secret, so it lives in the keychain.
            Keychain.set(settings.password, forKey: Key.password)
        }
    )

    static let testValue = Self()
}

extension DependencyValues {
    var playerSettingsClient: PlayerSettingsClient {
        get { self[PlayerSettingsClient.self] }
        set { self[PlayerSettingsClient.self] = newValue }
    }
}
