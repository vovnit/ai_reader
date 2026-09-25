import ComposableArchitecture
import Foundation

/// What a player reports about itself.
struct PlayerStatus: Equatable, Sendable {
    enum PlayState: Sendable {
        case playing, paused, stopped
    }

    let state: PlayState
    /// Seconds into the item.
    let time: TimeInterval
    /// Names the item being played; changes when the file does.
    let item: String?

    static let stopped = Self(state: .stopped, time: 0, item: nil)
}

/// Where a player was found on the network.
struct PlayerAddress: Equatable, Sendable {
    let host: String
    let port: Int
}

enum PlayerError: LocalizedError {
    case unreachable(PlayerSettings.Kind)
    case unauthorized(PlayerSettings.Kind)

    var errorDescription: String? {
        switch self {
        case let .unreachable(kind): "\(kind.title) is not answering. Turn on its remote control and open a video."
        case let .unauthorized(kind): "\(kind.title) rejected the user name or password."
        }
    }
}

/// Follows and pauses whichever player the settings name.
@DependencyClient
struct PlayerClient: Sendable {
    var status: @Sendable (_ settings: PlayerSettings) async throws -> PlayerStatus
    /// The file being played, as the player names it: a local file for VLC,
    /// a `dav://` or other remote address for Kodi.
    var nowPlaying: @Sendable (_ settings: PlayerSettings) async throws -> URL?
    var pause: @Sendable (_ settings: PlayerSettings) async throws -> Void
    var resume: @Sendable (_ settings: PlayerSettings) async throws -> Void
    /// Waits for a Kodi to announce itself on the local network.
    var findKodi: @Sendable () async -> PlayerAddress? = { nil }
}

extension PlayerClient: DependencyKey {
    static let liveValue = Self(
        status: { settings in
            switch settings.kind {
            case .vlc: try await VLC.status(settings)
            case .kodi: try await Kodi.status(settings)
            }
        },
        nowPlaying: { settings in
            switch settings.kind {
            case .vlc: try await VLC.nowPlaying(settings)
            case .kodi: try await Kodi.nowPlaying(settings)
            }
        },
        pause: { settings in
            switch settings.kind {
            case .vlc: try await VLC.pause(settings)
            case .kodi: try await Kodi.pause(settings)
            }
        },
        resume: { settings in
            switch settings.kind {
            case .vlc: try await VLC.resume(settings)
            case .kodi: try await Kodi.resume(settings)
            }
        },
        findKodi: { await KodiFinder.find() }
    )

    static let testValue = Self()

    /// Sends one request to the player's web server and returns the JSON it
    /// answers with. Both players answer 401 to a wrong password.
    static func json(_ request: URLRequest, _ settings: PlayerSettings) async throws -> Any {
        var request = request
        request.timeoutInterval = 5
        request.setValue(settings.authorization, forHTTPHeaderField: "Authorization")
        let (data, response): (Data, URLResponse)
        do {
            (data, response) = try await URLSession.shared.data(for: request)
        } catch {
            throw PlayerError.unreachable(settings.kind)
        }
        guard let http = response as? HTTPURLResponse else { throw PlayerError.unreachable(settings.kind) }
        guard http.statusCode != 401 else { throw PlayerError.unauthorized(settings.kind) }
        guard http.statusCode == 200 else { throw PlayerError.unreachable(settings.kind) }
        return try JSONSerialization.jsonObject(with: data)
    }
}

extension DependencyValues {
    var playerClient: PlayerClient {
        get { self[PlayerClient.self] }
        set { self[PlayerClient.self] = newValue }
    }
}
