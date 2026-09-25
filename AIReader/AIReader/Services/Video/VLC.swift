import Foundation

/// VLC's web interface: the player's state as JSON at `status.json`, commands
/// as its `command` query parameter, the playlist at `playlist.json`.
enum VLC {
    static func status(_ settings: PlayerSettings) async throws -> PlayerStatus {
        guard let object = try await request("status.json", settings) as? [String: Any] else {
            throw PlayerError.unreachable(.vlc)
        }
        let state: PlayerStatus.PlayState = switch object["state"] as? String {
        case "playing": .playing
        case "paused": .paused
        default: .stopped
        }
        // `time` is whole seconds; `position` keeps the fraction.
        let time = (object["time"] as? Double) ?? 0
        let length = (object["length"] as? Double) ?? 0
        let position = (object["position"] as? Double) ?? 0
        return PlayerStatus(
            state: state,
            time: length > 0 ? position * length : time,
            item: (object["currentplid"] as? Int).map(String.init)
        )
    }

    static func nowPlaying(_ settings: PlayerSettings) async throws -> URL? {
        let playlist = try await request("playlist.json", settings)
        return currentURI(in: playlist).flatMap(URL.init(string:))
    }

    static func pause(_ settings: PlayerSettings) async throws {
        _ = try await request("status.json", command: "pl_forcepause", settings)
    }

    static func resume(_ settings: PlayerSettings) async throws {
        _ = try await request("status.json", command: "pl_forceresume", settings)
    }

    private static func request(_ file: String, command: String? = nil, _ settings: PlayerSettings) async throws -> Any {
        guard let base = settings.baseURL,
              var components = URLComponents(url: base.appendingPathComponent("requests/\(file)"), resolvingAgainstBaseURL: false)
        else { throw PlayerError.unreachable(.vlc) }
        components.queryItems = command.map { [URLQueryItem(name: "command", value: $0)] }
        guard let url = components.url else { throw PlayerError.unreachable(.vlc) }
        return try await PlayerClient.json(URLRequest(url: url), settings)
    }

    /// The playlist is a tree; the item being played is marked `current`.
    private static func currentURI(in node: Any) -> String? {
        guard let object = node as? [String: Any] else { return nil }
        if object["current"] != nil, let uri = object["uri"] as? String { return uri }
        for child in object["children"] as? [Any] ?? [] {
            if let uri = currentURI(in: child) { return uri }
        }
        return nil
    }
}
