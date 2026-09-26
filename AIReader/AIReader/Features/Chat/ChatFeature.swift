import ComposableArchitecture
import Foundation

/// A conversation about something in front of the reader: the page, a word
/// just explained, what the book says about a name. The context is sent
/// once, with the first question, so that follow-ups cost only the thread so
/// far. The model may search the book while answering.
@Reducer
struct ChatFeature {
    @ObservableState
    struct State: Equatable {
        /// Sent to the model with the first question, as `ChatPrompt` builds it.
        let context: String
        /// Shown in the empty thread: what this conversation is for.
        let hint: String
        /// The books the model may search while answering.
        let scope: ReadingScope
        var turns: [ChatTurn] = []
        var draft = ""
        var isAnswering = false
        var errorMessage: String?

        var canSend: Bool {
            !draft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty && !isAnswering
        }

        static func aboutPage(_ page: String, scope: ReadingScope) -> Self {
            Self(
                context: ChatPrompt.pageContext(page),
                hint: "Ask about this page — a sentence you can’t parse, a word’s role, what is going on.",
                scope: scope
            )
        }

        static func aboutWord(_ context: LookupContext, explanation: WordExplanation, scope: ReadingScope) -> Self {
            Self(
                context: ChatPrompt.wordContext(
                    word: context.word,
                    sentence: context.sentence,
                    explanation: explanation
                ),
                hint: "Ask about this word — another example, a nuance, how it differs from a similar one.",
                scope: scope
            )
        }

        static func aboutXRay(term: String, answer: String, scope: ReadingScope) -> Self {
            Self(
                context: ChatPrompt.xrayContext(term: term, answer: answer),
                hint: "Ask about this — who they are to someone else, where it was first mentioned, what it stands for.",
                scope: scope
            )
        }
    }

    enum Action: BindableAction {
        case binding(BindingAction<State>)
        case sendTapped
        case answered(String)
        case failed(String)
    }

    @Dependency(\.aiSettingsClient) var aiSettings
    @Dependency(\.uuid) var uuid
    @Dependency(\.webSearchSettingsClient) var webSearchSettings

    var body: some ReducerOf<Self> {
        BindingReducer()
        Reduce { state, action in
            switch action {
            case .sendTapped:
                guard state.canSend else { return .none }
                let question = state.draft.trimmingCharacters(in: .whitespacesAndNewlines)
                state.turns.append(ChatTurn(id: uuid(), isReader: true, text: question))
                state.draft = ""
                state.isAnswering = true
                state.errorMessage = nil

                let settings = aiSettings.load()
                var messages = ChatPrompt.messages(context: state.context, turns: state.turns, language: settings.language)
                let tools = ToolRunner.Tools(scope: state.scope, web: webSearchSettings.load())
                return .run { send in
                    do {
                        let reply = try await ToolRunner.converse(
                            settings: settings,
                            messages: &messages,
                            tools: [SearchTool.tool],
                            jsonMode: false,
                            available: tools
                        )
                        await send(.answered(reply.content ?? ""))
                    } catch is CancellationError {
                    } catch {
                        await send(.failed(error.localizedDescription))
                    }
                }

            case let .answered(text):
                state.isAnswering = false
                state.turns.append(ChatTurn(id: uuid(), isReader: false, text: text))
                return .none

            case let .failed(message):
                state.isAnswering = false
                state.errorMessage = message
                return .none

            case .binding:
                return .none
            }
        }
    }
}
