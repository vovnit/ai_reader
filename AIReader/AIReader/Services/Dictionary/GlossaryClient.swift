import ComposableArchitecture
import Foundation

/// A book's offline glossary: the dictionary named after the book, filled a
/// batch at a time as the model's definitions arrive — so lookups use it while
/// it fills, and a run that stops keeps what it paid for. One added from the
/// word list DictionaryTool's `book_glossary.py` writes is continued.
@DependencyClient
struct GlossaryClient: Sendable {
    /// The forms the book's glossary defines so far.
    var definedForms: @Sendable (_ title: String) async -> Set<String> = { _ in [] }
    /// Form → definition for a batch, from the model.
    var define: @Sendable (_ words: [BookWord]) async throws -> [String: String]
    /// Files definitions in the book's glossary, beginning it if need be.
    var add: @Sendable (_ definitions: [String: String], _ title: String) async throws -> Void

    /// A book's glossary is the dictionary of this name.
    static func name(forTitle title: String) -> String {
        "\(title) glossary"
    }

    enum GlossaryError: LocalizedError {
        case unreadableAnswer

        var errorDescription: String? { "The model's answer could not be read." }
    }
}

extension GlossaryClient: DependencyKey {
    static var liveValue: Self {
        Self(
            definedForms: { title in
                await GlossaryStore.shared.definedForms(name: name(forTitle: title))
            },
            define: { words in
                @Dependency(\.aiSettingsClient) var aiSettings
                let settings = aiSettings.load()
                let reply = try await ChatAPI.chat(
                    settings: settings,
                    messages: [
                        .system(GlossaryPrompt.system(language: settings.language)),
                        .user(GlossaryPrompt.question(words))
                    ],
                    jsonMode: true,
                    maxTokens: GlossaryPrompt.maxTokens
                )
                guard let definitions = GlossaryPrompt.definitions(in: reply.content ?? "", for: words) else {
                    throw GlossaryError.unreadableAnswer
                }
                return definitions
            },
            add: { definitions, title in
                try await GlossaryStore.shared.add(definitions, name: name(forTitle: title))
            }
        )
    }

    static let testValue = Self()
}

extension DependencyValues {
    var glossaryClient: GlossaryClient {
        get { self[GlossaryClient.self] }
        set { self[GlossaryClient.self] = newValue }
    }
}
