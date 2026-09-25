import ComposableArchitecture
import Foundation

/// The matching game over the words looked up so far: a few cards a round,
/// fronts against backs, and another round once they are all paired. Every
/// pair put together is recorded, so a card that was missed comes round
/// again sooner.
@Reducer
struct MatchFeature {
    static let cardsPerRound = 5

    @ObservableState
    struct State: Equatable {
        /// Narrows the cards to one book; nil plays with every word met.
        let bookID: Book.ID?
        /// Nothing to play until two words have been looked up.
        var round: MatchRound?
        var isLoaded = false
    }

    enum Action {
        case task
        case roundLoaded([Card])
        case frontTapped(Int)
        case backTapped(Int)
        case nextRoundTapped
    }

    @Dependency(\.cardClient) var cards
    @Dependency(\.withRandomNumberGenerator) var random

    var body: some ReducerOf<Self> {
        Reduce { state, action in
            switch action {
            case .task:
                guard !state.isLoaded else { return .none }
                return load(state.bookID)

            case .nextRoundTapped:
                return load(state.bookID)

            case let .roundLoaded(cards):
                state.isLoaded = true
                let due = Card.due(cards, count: Self.cardsPerRound)
                state.round = due.count < 2 ? nil : random { MatchRound(cards: due, using: &$0) }
                return .none

            case let .frontTapped(card):
                guard let pick = state.round?.pickFront(card) else { return .none }
                return record(pick, in: state)

            case let .backTapped(card):
                guard let pick = state.round?.pickBack(card) else { return .none }
                return record(pick, in: state)
            }
        }
    }

    private func load(_ bookID: Book.ID?) -> Effect<Action> {
        .run { send in await send(.roundLoaded(await cards.all(bookID: bookID))) }
    }

    private func record(_ pick: MatchRound.Pick, in state: State) -> Effect<Action> {
        guard let round = state.round else { return .none }
        let front = round.cards[pick.front].lookupID
        let back = round.cards[pick.back].lookupID
        return .run { _ in
            if pick.matched {
                await cards.record(lookupID: front, correct: true)
            } else {
                // The two were confused with each other; both need another look.
                await cards.record(lookupID: front, correct: false)
                await cards.record(lookupID: back, correct: false)
            }
        }
    }
}
