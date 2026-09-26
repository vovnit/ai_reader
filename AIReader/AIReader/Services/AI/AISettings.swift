import ComposableArchitecture
import Foundation

/// Which model answers word lookups, and where to reach it. Any
/// OpenAI-compatible endpoint works: the base URL is extended with
/// `/chat/completions` and `/models`.
struct AISettings: Equatable, Sendable {
    var endpoint: String
    var apiKey: String
    var model: String
    /// The language explanations and answers are written in, as the model
    /// is told it: a name such as "Russian" or "English".
    var language: String

    /// Not a real address. Pointing the app here answers lookups from
    /// `MockAI` instead of the network, which is how the app is tested
    /// without spending requests.
    static let mockEndpoint = "mock://ai"

    static let `default` = Self(
        endpoint: "https://api.mistral.ai/v1",
        apiKey: "",
        model: "mistral-medium-3.5",
        language: "Russian"
    )

    var usesMock: Bool {
        endpoint.trimmingCharacters(in: .whitespacesAndNewlines) == Self.mockEndpoint
    }

    var chatURL: URL? { url(forPath: "chat/completions") }
    var modelsURL: URL? { url(forPath: "models") }

    private func url(forPath path: String) -> URL? {
        var base = endpoint.trimmingCharacters(in: .whitespacesAndNewlines)
        while base.hasSuffix("/") { base.removeLast() }
        return URL(string: "\(base)/\(path)")
    }
}

/// Reads and writes the endpoint, token, model and answer language.
@DependencyClient
struct AISettingsClient: Sendable {
    var load: @Sendable () -> AISettings = { .default }
    var save: @Sendable (_ settings: AISettings) -> Void
}

extension AISettingsClient: DependencyKey {
    private enum Key {
        static let endpoint = "aiEndpoint"
        static let apiKey = "aiAPIKey"
        static let model = "aiModel"
        static let language = "aiLanguage"
    }

    static let liveValue = Self(
        load: {
            migrateToSharedDefaults()
            return AISettings(
                endpoint: stored(Key.endpoint) ?? AISettings.default.endpoint,
                apiKey: token(),
                model: stored(Key.model) ?? AISettings.default.model,
                // A cleared field would leave the model no language to answer in.
                language: stored(Key.language) ?? AISettings.default.language
            )
        },
        save: { settings in
            let defaults = AppGroup.defaults
            defaults.set(settings.endpoint, forKey: Key.endpoint)
            defaults.set(settings.model, forKey: Key.model)
            defaults.set(settings.language, forKey: Key.language)
            // The token is the one secret here, so it lives in the keychain
            // rather than in preferences.
            Keychain.set(settings.apiKey, forKey: Key.apiKey)
        }
    )

    /// Settings live in the shared suite so the extension reads the same ones.
    /// Launch arguments, which arrive through the standard defaults, still
    /// override them for that one run.
    private static func stored(_ key: String) -> String? {
        let value = UserDefaults.standard.string(forKey: key) ?? AppGroup.defaults.string(forKey: key)
        return value.flatMap { $0.isEmpty ? nil : $0 }
    }

    /// Earlier builds kept the endpoint and model in the app's own preferences.
    /// Move them to the shared suite once, so they stop shadowing it.
    private static func migrateToSharedDefaults() {
        let standard = UserDefaults.standard
        guard let domain = Bundle.main.bundleIdentifier,
              let legacy = standard.persistentDomain(forName: domain)
        else { return }
        for key in [Key.endpoint, Key.model] {
            guard let value = legacy[key] as? String else { continue }
            if AppGroup.defaults.string(forKey: key) == nil {
                AppGroup.defaults.set(value, forKey: key)
            }
            standard.removeObject(forKey: key)
        }
    }

    /// An emptied token is a choice — locally run endpoints often want none —
    /// so the fallback only applies before one has ever been stored:
    /// `MISTRAL_API_KEY` in the run scheme's environment, for development.
    /// No token ships with the app.
    private static func token() -> String {
        if let migrated = migrateTokenFromPreferences() { return migrated }
        if let stored = Keychain.string(forKey: Key.apiKey) { return stored }

        return ProcessInfo.processInfo.environment["MISTRAL_API_KEY"] ?? ""
    }

    /// Earlier builds kept the token in preferences. Move any leftover into the
    /// keychain the first time it is read, and stop storing it in the clear.
    ///
    /// Only the app's own persisted preferences are considered: reading through
    /// `string(forKey:)` would also pick up launch arguments, and migrating one
    /// of those would overwrite the stored token.
    private static func migrateTokenFromPreferences() -> String? {
        let defaults = UserDefaults.standard
        guard let domain = Bundle.main.bundleIdentifier,
              let legacy = defaults.persistentDomain(forName: domain)?[Key.apiKey] as? String
        else { return nil }

        defaults.removeObject(forKey: Key.apiKey)
        guard Keychain.string(forKey: Key.apiKey) == nil else { return nil }
        Keychain.set(legacy, forKey: Key.apiKey)
        return legacy
    }

    static let testValue = Self()
}

extension DependencyValues {
    var aiSettingsClient: AISettingsClient {
        get { self[AISettingsClient.self] }
        set { self[AISettingsClient.self] = newValue }
    }
}
