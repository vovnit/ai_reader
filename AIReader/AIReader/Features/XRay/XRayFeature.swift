import ComposableArchitecture
import Foundation

/// What the book itself says a name or a word is: the passages where it has
/// appeared so far are gathered and the model is asked to read them. Nothing
/// past the page on screen is shown or sent, so nothing is given away.
@Reducer
struct XRayFeature {
    @ObservableState
    struct State: Equatable {
        /// Empty until the reader has typed one, when opened from the menu.
        var term: String
        let scope: ReadingScope
        var draft = ""
        /// The passages the answer was drawn from, in reading order.
        var passages: [SearchHit] = []
        var answer = ""
        var errorMessage: String?
        var isWorking = false

        var isAsking: Bool { term.isEmpty }
        var severalBooks: Bool { scope.corpus?.severalBooks ?? false }
    }

    enum Action: BindableAction {
        case binding(BindingAction<State>)
        case task
        case termSubmitted
        case finished(passages: [SearchHit], answer: String, error: String?)
        case hitTapped(SearchHit)
        case askTapped
        case delegate(Delegate)

        enum Delegate: Equatable {
            case jump(BookPosition)
            case ask(ChatFeature.State)
        }
    }

    @Dependency(\.aiSettingsClient) var aiSettings
    @Dependency(\.webSearchSettingsClient) var webSearchSettings

    var body: some ReducerOf<Self> {
        BindingReducer()
        Reduce { state, action in
            switch action {
            case .task:
                return start(&state)

            case .termSubmitted:
                let term = state.draft.trimmingCharacters(in: .whitespacesAndNewlines)
                guard !term.isEmpty else { return .none }
                state.term = term
                return start(&state)

            case let .finished(passages, answer, error):
                state.isWorking = false
                state.passages = passages
                state.answer = answer
                state.errorMessage = error
                return .none

            case let .hitTapped(hit):
                return .send(.delegate(.jump(hit.position)))

            case .askTapped:
                return .send(.delegate(.ask(.aboutXRay(term: state.term, answer: state.answer, scope: state.scope))))

            case .binding, .delegate:
                return .none
            }
        }
    }

    private func start(_ state: inout State) -> Effect<Action> {
        guard !state.isAsking, !state.isWorking, state.answer.isEmpty else { return .none }
        state.isWorking = true
        let term = state.term
        let scope = state.scope
        let settings = aiSettings.load()
        let tools = ToolRunner.Tools(scope: scope, web: webSearchSettings.load())
        return .run { send in
            guard let corpus = scope.corpus else {
                await send(.finished(passages: [], answer: "", error: "No book is open to read from."))
                return
            }
            let passages = await corpus.search(term, limit: XRayPrompt.passageLimit, upTo: scope.upTo)
            var answer = ""
            var error: String?
            do {
                var messages = XRayPrompt.messages(term: term, hits: passages, severalBooks: corpus.severalBooks, language: settings.language)
                answer = try await ToolRunner.converse(
                    settings: settings,
                    messages: &messages,
                    tools: [SearchTool.tool],
                    jsonMode: false,
                    available: tools
                ).content ?? ""
            } catch is CancellationError {
                return
            } catch let failure {
                error = failure.localizedDescription
            }
            let problems = await corpus.errors
            if !problems.isEmpty {
                error = ([error].compactMap { $0 } + problems).joined(separator: "\n")
            }
            await send(.finished(passages: passages, answer: answer, error: error))
        }
    }
}
