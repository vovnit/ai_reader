import ComposableArchitecture
import Foundation

/// The book's table of contents, the entry the page falls under marked; an
/// entry tapped turns the reader there.
@Reducer
struct ContentsFeature {
    @ObservableState
    struct State: Equatable {
        let bookID: Book.ID
        let entries: [ContentsEntry]
        /// The entry the page on screen falls under, if any.
        let current: Int?
    }

    enum Action {
        case entryTapped(Int)
        case delegate(Delegate)

        enum Delegate: Equatable {
            case jump(BookPosition)
        }
    }

    var body: some ReducerOf<Self> {
        Reduce { state, action in
            switch action {
            case let .entryTapped(index):
                guard state.entries.indices.contains(index) else { return .none }
                let entry = state.entries[index]
                return .send(.delegate(.jump(BookPosition(bookID: state.bookID, chapter: entry.chapter, offset: entry.offset))))

            case .delegate:
                return .none
            }
        }
    }
}
