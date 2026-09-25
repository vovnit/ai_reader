import ComposableArchitecture
import Foundation

/// One word lookup: read the cache, otherwise ask the explainer and store the
/// answer. The dictionary's own entry for the lemma comes with it, so the
/// reader can see what the answer was drawn from; from here the word can be
/// X-rayed in the book or talked over with the model.
@Reducer
struct LookupFeature {
    @Reducer
    enum Path {
        case entry(DictionaryEntryFeature)
        case xray(XRayFeature)
        case chat(ChatFeature)
    }

    @ObservableState
    struct State: Equatable, Identifiable {
        let id: UUID
        let context: LookupContext
        /// What the model may search while explaining; none from a screen
        /// with no book open.
        let scope: ReadingScope
        var explanation: WordExplanation?
        /// The articles under the lemma; empty when the dictionary has no such
        /// headword, as after a guess.
        var entry: [DictionaryLookup.Article] = []
        var errorMessage: String?
        var path = StackState<Path.State>()

        init(id: UUID, context: LookupContext, scope: ReadingScope = .none) {
            self.id = id
            self.context = context
            self.scope = scope
        }
    }

    enum Action {
        case task
        case explained(WordExplanation, entry: [DictionaryLookup.Article])
        case failed(String)
        case speakTapped(String)
        case doneTapped
        case entryTapped
        case xrayTapped
        case askTapped
        case path(StackActionOf<Path>)
        case delegate(Delegate)

        enum Delegate: Equatable {
            case jump(BookPosition)
        }
    }

    @Dependency(\.lookupCacheClient) var cache
    @Dependency(\.dictionaryClient) var dictionary
    @Dependency(\.dismiss) var dismiss
    @Dependency(\.speechClient) var speech
    @Dependency(\.wordExplainerClient) var explainer

    var body: some ReducerOf<Self> {
        Reduce { state, action in
            switch action {
            case .task:
                guard state.explanation == nil else { return .none }
                let context = state.context
                let scope = state.scope
                return .run { send in
                    if let cached = await cache.cached(context: context) {
                        await send(.explained(cached, entry: await dictionary.articles(lemma: cached.lemma)))
                        return
                    }
                    do {
                        let explanation = try await explainer.explain(
                            word: context.word,
                            sentence: context.sentence,
                            scope: scope
                        )
                        await cache.save(context: context, explanation: explanation)
                        await send(.explained(explanation, entry: await dictionary.articles(lemma: explanation.lemma)))
                    } catch is CancellationError {
                    } catch {
                        await send(.failed(error.localizedDescription))
                    }
                }

            case let .explained(explanation, entry):
                state.explanation = explanation
                state.entry = entry
                state.errorMessage = nil
                return .none

            case let .failed(message):
                state.errorMessage = message
                return .none

            case let .speakTapped(text):
                speech.speak(text: text, language: state.context.language)
                return .none

            case .doneTapped:
                return .run { _ in await dismiss() }

            case .entryTapped:
                guard let explanation = state.explanation, !state.entry.isEmpty else { return .none }
                state.path.append(.entry(DictionaryEntryFeature.State(lemma: explanation.lemma, articles: state.entry)))
                return .none

            case .xrayTapped:
                state.path.append(.xray(XRayFeature.State(term: state.context.word, scope: state.scope)))
                return .none

            case .askTapped:
                guard let explanation = state.explanation else { return .none }
                state.path.append(.chat(.aboutWord(state.context, explanation: explanation, scope: state.scope)))
                return .none

            case let .path(.element(id: _, action: .xray(.delegate(.jump(position))))):
                return .send(.delegate(.jump(position)))

            case let .path(.element(id: _, action: .xray(.delegate(.ask(chat))))):
                state.path.append(.chat(chat))
                return .none

            case .path, .delegate:
                return .none
            }
        }
        .forEach(\.path, action: \.path)
    }
}

extension LookupFeature.Path.State: Equatable {}
