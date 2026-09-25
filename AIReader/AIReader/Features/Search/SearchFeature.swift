import ComposableArchitecture
import Foundation

/// Finding a phrase in the book being read and the rest of its group. The
/// reader's own search covers the whole text — what to know ahead of time is
/// their choice; only the model is kept to the pages read.
@Reducer
struct SearchFeature {
    /// How many hits are listed.
    static let limit = 100

    @ObservableState
    struct State: Equatable {
        let scope: ReadingScope
        /// "this book" or the group's name, for the empty screen.
        let covers: String
        var draft = ""
        var query = ""
        var hits: [SearchHit] = []
        var isSearching = false
        var errorMessage: String?

        var severalBooks: Bool { scope.corpus?.severalBooks ?? false }
    }

    enum Action: BindableAction {
        case binding(BindingAction<State>)
        case searchTapped
        case found([SearchHit], error: String?)
        case hitTapped(SearchHit)
        case delegate(Delegate)

        enum Delegate: Equatable {
            case jump(BookPosition)
        }
    }

    var body: some ReducerOf<Self> {
        BindingReducer()
        Reduce { state, action in
            switch action {
            case .searchTapped:
                let query = state.draft.trimmingCharacters(in: .whitespacesAndNewlines)
                guard !query.isEmpty, !state.isSearching, let corpus = state.scope.corpus else { return .none }
                state.query = query
                state.isSearching = true
                state.errorMessage = nil
                return .run { send in
                    let hits = await corpus.search(query, limit: Self.limit)
                    let errors = await corpus.errors
                    await send(.found(hits, error: errors.isEmpty ? nil : errors.joined(separator: "\n")))
                }

            case let .found(hits, error):
                state.isSearching = false
                state.hits = hits
                state.errorMessage = error
                return .none

            case let .hitTapped(hit):
                return .send(.delegate(.jump(hit.position)))

            case .binding, .delegate:
                return .none
            }
        }
    }
}
