import ComposableArchitecture
import Foundation
import SQLiteData

/// Every word looked up so far, newest first — the vocabulary the reader has
/// actually met, rather than a list someone else chose. From here the words
/// are practised, or written out for Anki.
@Reducer
struct WordsFeature {
    @ObservableState
    struct State: Equatable {
        /// Set to show only one book's words; nil shows every word met so far.
        var bookID: Book.ID?

        @FetchAll(Lookup.order { $0.lookedUpAt.desc() })
        var allLookups

        @Presents var match: MatchFeature.State?
        /// The cards written for Anki, while the exporter is up.
        var export: AnkiCardsDocument?

        var lookups: [Lookup] {
            guard let bookID else { return allLookups }
            return allLookups.filter { $0.bookID == bookID }
        }
    }

    enum Action: BindableAction {
        case binding(BindingAction<State>)
        case doneTapped
        case deleteTapped(Lookup)
        case speakTapped(Lookup)
        case practiceTapped
        case exportTapped
        case exportPrepared(AnkiCardsDocument)
        case match(PresentationAction<MatchFeature.Action>)
        case delegate(Delegate)

        enum Delegate {
            case dismiss
        }
    }

    @Dependency(\.lookupCacheClient) var cache
    @Dependency(\.libraryClient) var library
    @Dependency(\.speechClient) var speech

    var body: some ReducerOf<Self> {
        BindingReducer()
        Reduce { state, action in
            switch action {
            case .doneTapped:
                return .send(.delegate(.dismiss))

            case let .deleteTapped(lookup):
                return .run { _ in await cache.remove(lookup: lookup) }

            case let .speakTapped(lookup):
                speech.speak(text: lookup.word, language: lookup.language)
                return .none

            case .practiceTapped:
                state.match = MatchFeature.State(bookID: state.bookID)
                return .none

            case .exportTapped:
                let cards = state.lookups.map { Card(lookup: $0) }
                let bookID = state.bookID
                return .run { send in
                    // One book's words go to a deck of their own under the app's.
                    var deck = "AIReader"
                    if let bookID, let book = await library.find(bookID: bookID) { deck += "::\(book.title)" }
                    await send(.exportPrepared(AnkiCardsDocument(text: AnkiExport.text(cards: cards, deck: deck))))
                }

            case let .exportPrepared(document):
                state.export = document
                return .none

            case .binding, .delegate, .match:
                return .none
            }
        }
        .ifLet(\.$match, action: \.match) {
            MatchFeature()
        }
    }
}
