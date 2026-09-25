import ComposableArchitecture
import Foundation
import SQLiteData

/// Which group a book belongs to: one of the groups there are, none, or a
/// new one typed in.
@Reducer
struct GroupPickerFeature {
    @ObservableState
    struct State: Equatable {
        let book: Book
        @FetchAll(BookGroup.order { $0.name })
        var groups
        var newName = ""
    }

    enum Action: BindableAction {
        case binding(BindingAction<State>)
        case groupTapped(BookGroup.ID?)
        case createTapped
        case cancelTapped
        case delegate(Delegate)

        enum Delegate {
            case dismiss
        }
    }

    @Dependency(\.groupClient) var groups

    var body: some ReducerOf<Self> {
        BindingReducer()
        Reduce { state, action in
            switch action {
            case let .groupTapped(groupID):
                let bookID = state.book.id
                return .run { send in
                    try? await groups.assign(bookID: bookID, groupID: groupID)
                    await send(.delegate(.dismiss))
                }

            case .createTapped:
                let name = state.newName.trimmingCharacters(in: .whitespacesAndNewlines)
                guard !name.isEmpty else { return .none }
                let bookID = state.book.id
                return .run { send in
                    if let groupID = try? await groups.named(name: name) {
                        try? await groups.assign(bookID: bookID, groupID: groupID)
                    }
                    await send(.delegate(.dismiss))
                }

            case .cancelTapped:
                return .send(.delegate(.dismiss))

            case .binding, .delegate:
                return .none
            }
        }
    }
}
