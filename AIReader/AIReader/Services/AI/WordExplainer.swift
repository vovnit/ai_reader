import ComposableArchitecture
import Foundation

/// Explains a word by handing the dictionary's answer to the model and letting
/// it ask for more entries — or search the book, or the web — until it can
/// commit to a meaning.
struct WordExplainer: Sendable {
    enum ExplainError: LocalizedError {
        case unreadableAnswer

        var errorDescription: String? { "The model's answer could not be read." }
    }

    let dictionary: DictionaryClient
    let settings: AISettings
    let web: WebSearchSettings
    let scope: ReadingScope

    func explain(word: String, sentence: String) async throws -> WordExplanation {
        var messages: [ChatMessage] = [
            .system(ExplanationPrompt.system),
            .user(
                ExplanationPrompt.question(
                    word: word,
                    sentence: sentence,
                    lookup: await dictionary.lookup(word)
                )
            )
        ]
        let reply = try await ToolRunner.converse(
            settings: settings,
            messages: &messages,
            tools: ExplanationPrompt.tools,
            jsonMode: true,
            available: ToolRunner.Tools(scope: scope, dictionary: dictionary, web: web)
        )
        guard let explanation = Self.decode(reply.content) else {
            throw ExplainError.unreadableAnswer
        }
        return explanation
    }

    private static func decode(_ content: String?) -> WordExplanation? {
        guard let content,
              let start = content.firstIndex(of: "{"),
              let end = content.lastIndex(of: "}")
        else { return nil }
        let json = Data(content[start...end].utf8)
        return try? JSONDecoder().decode(WordExplanation.self, from: json)
    }
}

/// Looks a word up and explains it. `scope` is what the model may search
/// while explaining; none from a screen with no book open.
@DependencyClient
struct WordExplainerClient: Sendable {
    var explain: @Sendable (_ word: String, _ sentence: String, _ scope: ReadingScope) async throws -> WordExplanation
}

extension WordExplainerClient: DependencyKey {
    static var liveValue: Self {
        Self { word, sentence, scope in
            @Dependency(\.aiSettingsClient) var aiSettings
            @Dependency(\.dictionaryClient) var dictionary
            @Dependency(\.webSearchSettingsClient) var webSearchSettings
            return try await WordExplainer(
                dictionary: dictionary,
                settings: aiSettings.load(),
                web: webSearchSettings.load(),
                scope: scope
            ).explain(word: word, sentence: sentence)
        }
    }
}

extension DependencyValues {
    var wordExplainerClient: WordExplainerClient {
        get { self[WordExplainerClient.self] }
        set { self[WordExplainerClient.self] = newValue }
    }
}
