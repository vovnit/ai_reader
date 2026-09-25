import Foundation

/// Kodi's JSON-RPC over its web server. Video always plays in player 1, so
/// one batched request asks for everything the poll needs.
enum Kodi {
    private static let videoPlayer = 1

    static func status(_ settings: PlayerSettings) async throws -> PlayerStatus {
        let answers = try await call([
            ("Player.GetActivePlayers", [:]),
            ("Player.GetProperties", ["playerid": videoPlayer, "properties": ["time", "speed"]]),
            ("Player.GetItem", ["playerid": videoPlayer, "properties": ["file"]]),
        ], settings)
        let players = answers[0] as? [[String: Any]] ?? []
        guard players.contains(where: { $0["playerid"] as? Int == videoPlayer }),
              let properties = answers[1] as? [String: Any]
        else { return .stopped }
        let speed = (properties["speed"] as? Double) ?? 0
        let item = (answers[2] as? [String: Any])?["item"] as? [String: Any]
        return PlayerStatus(
            state: speed == 0 ? .paused : .playing,
            time: seconds(properties["time"]),
            item: item?["file"] as? String
        )
    }

    static func nowPlaying(_ settings: PlayerSettings) async throws -> URL? {
        let answer = try await call([("Player.GetItem", ["playerid": videoPlayer, "properties": ["file"]])], settings)[0]
        let item = (answer as? [String: Any])?["item"] as? [String: Any]
        return (item?["file"] as? String).flatMap(URL.init(string:))
    }

    static func pause(_ settings: PlayerSettings) async throws {
        _ = try await call([("Player.PlayPause", ["playerid": videoPlayer, "play": false])], settings)
    }

    static func resume(_ settings: PlayerSettings) async throws {
        _ = try await call([("Player.PlayPause", ["playerid": videoPlayer, "play": true])], settings)
    }

    /// Kodi reports time as hours, minutes, seconds and milliseconds.
    private static func seconds(_ time: Any?) -> TimeInterval {
        guard let parts = time as? [String: Any] else { return 0 }
        func part(_ name: String) -> Double { (parts[name] as? Double) ?? 0 }
        return part("hours") * 3600 + part("minutes") * 60 + part("seconds") + part("milliseconds") / 1000
    }

    /// Sends the calls as one batch and returns their results in the same
    /// order; a call Kodi answered with an error gives nil.
    private static func call(_ calls: [(method: String, params: [String: Any])], _ settings: PlayerSettings) async throws -> [Any?] {
        guard let url = settings.baseURL?.appendingPathComponent("jsonrpc") else { throw PlayerError.unreachable(.kodi) }
        let body = calls.enumerated().map { index, call in
            ["jsonrpc": "2.0", "id": index, "method": call.method, "params": call.params] as [String: Any]
        }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.httpBody = try JSONSerialization.data(withJSONObject: body)
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        guard let answers = try await PlayerClient.json(request, settings) as? [[String: Any]] else {
            throw PlayerError.unreachable(.kodi)
        }
        return calls.indices.map { index in
            answers.first { $0["id"] as? Int == index }?["result"]
        }
    }
}
