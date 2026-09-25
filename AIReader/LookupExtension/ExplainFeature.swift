import ComposableArchitecture
import Foundation

/// A lookup that started in another app's share sheet: the passage that came
/// in, and the explanation of the word chosen from it. There is no book, so
/// nothing is searched and the lookup is filed under no title.
@Reducer
struct ExplainFeature {
    @ObservableState
    struct State: Equatable {
        let passage: SharedPassage
        @Presents var lookup: LookupFeature.State?
        /// Set when there is nothing left to show and the extension should close.
        var isFinished = false
    }

    enum Action {
        case task
        case wordTapped(utf16Offset: Int)
        case doneTapped
        case lookup(PresentationAction<LookupFeature.Action>)
    }

    @Dependency(\.uuid) var uuid

    var body: some ReducerOf<Self> {
        Reduce { state, action in
            switch action {
            case .task:
                // A single shared word needs no tap: explain it straight away.
                guard let word = state.passage.word else { return .none }
                return explain(word, &state)

            case let .wordTapped(offset):
                guard let selection = WordContext(text: state.passage.text).selection(atUTF16Offset: offset)
                else { return .none }
                return explain(selection, &state)

            case .doneTapped:
                state.isFinished = true
                return .none

            case .lookup(.dismiss) where state.passage.word != nil:
                // Behind a single word's explanation there is nothing to return to.
                state.isFinished = true
                return .none

            case .lookup:
                return .none
            }
        }
        .ifLet(\.$lookup, action: \.lookup) {
            LookupFeature()
        }
    }

    private func explain(_ selection: WordContext.Selection, _ state: inout State) -> Effect<Action> {
        state.lookup = LookupFeature.State(
            id: uuid(),
            context: LookupContext(
                word: selection.word,
                sentence: selection.sentence,
                language: state.passage.language
            )
        )
        return .none
    }
}
