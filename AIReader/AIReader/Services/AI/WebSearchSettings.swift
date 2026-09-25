import ComposableArchitecture
import Foundation

/// How the model reaches the web: a Monid token, and which of the search
/// endpoints Monid brokers answers. Nothing is searched until a token is
/// given, since most endpoints are paid per call.
struct WebSearchSettings: Equatable, Sendable {
    var apiKey = ""
    /// Which endpoint answers, as `monid discover -q "web search"` lists
    /// them: a provider slug and an endpoint path.
    var provider = "tinyfish"
    var endpoint = "/search"
    /// What the endpoint is sent, as JSON: Monid's `input`, with the
    /// `queryParams`, `body` or `pathParams` that `monid inspect` lists.
    /// `$query` stands for the words searched and `$language` for the
    /// book's language.
    var input = #"{"queryParams": {"query": "$query", "language": "$language"}}"#

    var isConfigured: Bool {
        ![apiKey, provider, endpoint].contains { $0.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }
    }

    /// `input` with the query and language in place, escaped for JSON.
    func request(query: String, language: String) -> String {
        input
            .replacingOccurrences(of: "$query", with: Self.escaped(query))
            .replacingOccurrences(of: "$language", with: Self.escaped(language.isEmpty ? "en" : language))
    }

    /// Dumped as a JSON string and unquoted, so quotes and newlines in the
    /// query cannot break out of the template.
    private static func escaped(_ value: String) -> String {
        guard let data = try? JSONSerialization.data(withJSONObject: value, options: .fragmentsAllowed),
              let quoted = String(data: data, encoding: .utf8)
        else { return "" }
        return String(quoted.dropFirst().dropLast())
    }
}

/// Reads and writes the web search settings.
@DependencyClient
struct WebSearchSettingsClient: Sendable {
    var load: @Sendable () -> WebSearchSettings = { WebSearchSettings() }
    var save: @Sendable (_ settings: WebSearchSettings) -> Void
}

extension WebSearchSettingsClient: DependencyKey {
    private enum Key {
        static let apiKey = "webSearchAPIKey"
        static let provider = "webSearchProvider"
        static let endpoint = "webSearchEndpoint"
        static let input = "webSearchInput"
    }

    /// The provider, endpoint and input live in the shared suite so the
    /// Explain extension searches the same way; the token is a secret, so it
    /// lives in the keychain.
    static let liveValue = Self(
        load: {
            let defaults = AppGroup.defaults
            let fallback = WebSearchSettings()
            return WebSearchSettings(
                apiKey: Keychain.string(forKey: Key.apiKey) ?? fallback.apiKey,
                provider: defaults.string(forKey: Key.provider) ?? fallback.provider,
                endpoint: defaults.string(forKey: Key.endpoint) ?? fallback.endpoint,
                input: defaults.string(forKey: Key.input) ?? fallback.input
            )
        },
        save: { settings in
            let defaults = AppGroup.defaults
            defaults.set(settings.provider, forKey: Key.provider)
            defaults.set(settings.endpoint, forKey: Key.endpoint)
            defaults.set(settings.input, forKey: Key.input)
            Keychain.set(settings.apiKey, forKey: Key.apiKey)
        }
    )

    static let testValue = Self()
}

extension DependencyValues {
    var webSearchSettingsClient: WebSearchSettingsClient {
        get { self[WebSearchSettingsClient.self] }
        set { self[WebSearchSettingsClient.self] = newValue }
    }
}
