import ComposableArchitecture
import Foundation

/// Walks the WebDAV server folder by folder until a subtitle file is chosen.
@Reducer
struct WebDAVPickerFeature {
    @ObservableState
    struct State: Equatable {
        var folder: URL
        var entries: [WebDAVEntry] = []
        var isLoading = false
        var errorMessage: String?

        var title: String {
            folder.lastPathComponent.isEmpty || folder.lastPathComponent == "/" ? "Server" : folder.lastPathComponent
        }
    }

    enum Action {
        case task
        case loaded(Result<[WebDAVEntry], any Error>)
        case entryTapped(WebDAVEntry)
        case upTapped
        case cancelTapped
        case delegate(Delegate)

        enum Delegate: Equatable {
            case picked(URL)
            case dismiss
        }
    }

    @Dependency(\.subtitleClient) var subtitles

    var body: some ReducerOf<Self> {
        Reduce { state, action in
            switch action {
            case .task:
                return list(&state)

            case let .loaded(.success(entries)):
                state.isLoading = false
                state.entries = entries
                return .none

            case let .loaded(.failure(error)):
                state.isLoading = false
                state.errorMessage = error.localizedDescription
                return .none

            case let .entryTapped(entry):
                guard entry.isFolder else { return .send(.delegate(.picked(entry.url))) }
                state.folder = entry.url
                return list(&state)

            case .upTapped:
                state.folder = state.folder.deletingLastPathComponent()
                return list(&state)

            case .cancelTapped:
                return .send(.delegate(.dismiss))

            case .delegate:
                return .none
            }
        }
    }

    private func list(_ state: inout State) -> Effect<Action> {
        state.isLoading = true
        state.errorMessage = nil
        state.entries = []
        let folder = state.folder
        return .run { send in
            await send(.loaded(Result { try await subtitles.browse(folder) }))
        }
    }
}
