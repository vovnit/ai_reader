import ComposableArchitecture
import Foundation

/// Carries a conversation with the model, answering the tools it calls —
/// the dictionary, the book search and the web — until it answers in words.
enum ToolRunner {
    enum RunError: LocalizedError {
        case gaveUp

        var errorDescription: String? { "The model kept searching without answering." }
    }

    /// What the model may reach for. A missing corpus or dictionary simply
    /// answers that tool with nothing. The web is offered only when it is set
    /// up, since a search there may be paid for — and always under the mock,
    /// which answers it itself.
    struct Tools: Sendable {
        var scope: ReadingScope = .none
        var dictionary: DictionaryClient?
        var web = WebSearchSettings()
    }

    /// How many rounds of tool calls the model gets before it must answer.
    static let budget = 3

    static func converse(
        settings: AISettings,
        messages: inout [ChatMessage],
        tools: [[String: Any]],
        jsonMode: Bool,
        available: Tools
    ) async throws -> ChatMessage {
        let offered = offered(tools, settings: settings, available: available)
        for _ in 0...budget {
            let reply = try await ChatAPI.chat(
                settings: settings,
                messages: messages,
                tools: offered,
                jsonMode: jsonMode
            )
            guard let calls = reply.toolCalls, !calls.isEmpty else { return reply }
            messages.append(reply)
            for call in calls {
                messages.append(.toolResult(await answer(call, settings: settings, available), callID: call.id))
            }
        }
        throw RunError.gaveUp
    }

    /// `tools` with the web added when it can be answered.
    private static func offered(_ tools: [[String: Any]], settings: AISettings, available: Tools) -> [[String: Any]] {
        guard settings.usesMock || available.web.isConfigured else { return tools }
        return tools + [WebSearchTool.tool]
    }

    private static func answer(_ call: ChatMessage.ToolCall, settings: AISettings, _ available: Tools) async -> String {
        switch call.function.name {
        case ExplanationPrompt.toolName:
            let word = ToolSchema.argument("word", in: call.function.arguments) ?? ""
            guard let dictionary = available.dictionary else {
                return DictionaryLookup(query: word).summary
            }
            return await dictionary.lookup(word).summary

        case SearchTool.toolName:
            let query = SearchTool.query(in: call.function.arguments)
            guard let corpus = available.scope.corpus, !query.isEmpty else {
                return SearchTool.summary(query: query, hits: [], severalBooks: false)
            }
            let hits = await corpus.search(query, limit: SearchTool.passageLimit, upTo: available.scope.upTo)
            return SearchTool.summary(query: query, hits: hits, severalBooks: corpus.severalBooks)

        case WebSearchTool.toolName:
            let query = WebSearchTool.query(in: call.function.arguments)
            guard !query.isEmpty else { return WebSearchTool.summary(query: query, output: nil) }
            // A failed search is told to the model, which can still answer
            // from what it has, rather than failing the whole conversation.
            do {
                // Pages in the book's language: a French name wants the French page.
                let language = available.scope.corpus?.books.first?.language ?? ""
                let output = settings.usesMock
                    ? MockAI.webSearch(query)
                    : try await WebSearch.search(settings: available.web, query: query, language: language)
                return WebSearchTool.summary(query: query, output: output)
            } catch {
                return "The web search failed: \(error.localizedDescription)"
            }

        default:
            return "There is no tool called “\(call.function.name)”."
        }
    }
}
