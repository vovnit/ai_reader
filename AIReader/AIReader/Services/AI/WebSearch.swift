import Foundation
import OSLog

/// Runs a web search through Monid, which brokers many providers' search
/// endpoints behind one token: the endpoint is run, a provider that works in
/// the background is polled, and the pages come back as the provider's own
/// JSON, whatever its shape.
enum WebSearch {
    enum SearchError: LocalizedError {
        case invalidInput
        case http(status: Int, message: String)
        case unreadableAnswer
        case notCompleted(status: String, reason: String)
        case providerFailed(status: Int, reason: String)
        case noRun
        case tooLong

        var errorDescription: String? {
            switch self {
            case .invalidInput: "The web search input is not valid JSON; check it in Settings."
            case let .http(status, message): "The web search failed (\(status)): \(message)"
            case .unreadableAnswer: "The web search's answer could not be read."
            case let .notCompleted(status, reason):
                "The web search did not complete (\(status)\(reason.isEmpty ? "" : ": \(reason)"))."
            case let .providerFailed(status, reason):
                "The search provider answered \(status)\(reason.isEmpty ? "." : ": \(reason)")"
            case .noRun: "The web search returned no run to wait for."
            case .tooLong: "The web search took too long."
            }
        }
    }

    private static let apiBase = "https://api.monid.ai/v1"
    /// How long a search in the background is waited for, all polls together.
    private static let waitSeconds = 60.0
    private static let logger = Logger(subsystem: "AIReader", category: "WebSearch")

    static func search(settings: WebSearchSettings, query: String, language: String) async throws -> Any {
        guard let data = settings.request(query: query, language: language).data(using: .utf8),
              let input = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        else { throw SearchError.invalidInput }

        // The input is sent as it is, `queryParams`, `body` and `pathParams`
        // inside it, the way Monid's own CLI sends what it is given.
        let request: [String: Any] = [
            "provider": settings.provider,
            "endpoint": settings.endpoint,
            "input": input
        ]
        var run = try await send(path: "/run", apiKey: settings.apiKey, body: request, timeout: 45)

        // A provider that works in the background is polled, a little less
        // often each time, the way Monid's own client does.
        var waited = 0.0
        var delay = 1.0
        while !isDone(run), waited < waitSeconds {
            try await Task.sleep(for: .seconds(delay))
            waited += delay
            delay = min(delay * 1.5, 10)
            guard let runID = run["runId"] as? String, !runID.isEmpty else { throw SearchError.noRun }
            run = try await send(path: "/runs/\(runID)", apiKey: settings.apiKey, body: nil, timeout: 30)
        }
        guard isDone(run) else { throw SearchError.tooLong }
        return try output(of: run)
    }

    private static func isDone(_ run: [String: Any]) -> Bool {
        ["COMPLETED", "FAILED", "BLOCKED"].contains(run["status"] as? String ?? "")
    }

    /// The provider's data out of a finished run, or why there is none.
    private static func output(of run: [String: Any]) throws -> Any {
        let status = run["status"] as? String ?? ""
        if status != "COMPLETED" {
            let reason = run["error"] as? String ?? (run["error"] as? [String: Any])?["message"] as? String ?? ""
            throw SearchError.notCompleted(status: status, reason: reason)
        }
        let provider = run["providerResponse"] as? [String: Any] ?? [:]
        let httpStatus = provider["httpStatus"] as? Int ?? 200
        if httpStatus >= 400 {
            let reason = (provider["error"] as? [String: Any])?["message"] as? String ?? ""
            throw SearchError.providerFailed(status: httpStatus, reason: reason)
        }
        // Every run costs something; the log says how much.
        if let cost = run["cost"] as? [String: Any] {
            logger.info(
                "web search \(run["provider"] as? String ?? "")\(run["endpoint"] as? String ?? "") cost \(cost["value"] as? Double ?? 0) \(cost["currency"] as? String ?? "")"
            )
        }
        if let data = provider["data"], !(data is NSNull) { return data }
        return run["output"] ?? NSNull()
    }

    private static func send(path: String, apiKey: String, body: [String: Any]?, timeout: TimeInterval) async throws -> [String: Any] {
        guard let url = URL(string: apiBase + path) else { throw SearchError.invalidInput }
        var request = URLRequest(url: url)
        request.timeoutInterval = timeout
        request.setValue("application/json", forHTTPHeaderField: "Accept")
        request.setValue("Bearer \(apiKey)", forHTTPHeaderField: "Authorization")
        if let body {
            request.httpMethod = "POST"
            request.setValue("application/json", forHTTPHeaderField: "Content-Type")
            request.httpBody = try JSONSerialization.data(withJSONObject: body)
        }

        let (data, response) = try await URLSession.shared.data(for: request)
        let parsed = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        if let http = response as? HTTPURLResponse, !(200...299).contains(http.statusCode) {
            let message = (parsed?["error"] as? [String: Any])?["message"] as? String
                ?? parsed?["message"] as? String
                ?? String(data: data, encoding: .utf8) ?? ""
            throw SearchError.http(status: http.statusCode, message: message)
        }
        guard let parsed else { throw SearchError.unreadableAnswer }
        return parsed
    }
}
