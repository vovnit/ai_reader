import ComposableArchitecture
import Foundation
import SQLiteData

/// The shelf: the books that have been added, the groups they are sorted
/// into, the books in the sync folder not fetched yet, and the door into the
/// reader.
@Reducer
struct LibraryFeature {
    @ObservableState
    struct State: Equatable {
        @FetchAll(Book.order { $0.addedAt.desc() })
        var books
        @FetchAll(BookGroup.order { $0.name })
        var groups
        @FetchAll(RemoteBook.order { $0.name })
        var remoteBooks

        var isImporterPresented = false
        var isSyncing = false
        var isSyncConfigured = false
        /// The files being fetched from the sync folder, by name.
        var downloading: Set<String> = []
        @Presents var alert: AlertState<Alert>?
        @Presents var groupPicker: GroupPickerFeature.State?
        @Presents var reader: ReaderFeature.State?
        @Presents var settings: SettingsFeature.State?
        @Presents var subtitles: SubtitlesFeature.State?
        @Presents var words: WordsFeature.State?

        /// The books in a group; for nil, the books in none.
        func books(in groupID: BookGroup.ID?) -> [Book] {
            books.filter { $0.groupID == groupID }
        }

        /// The books in the sync folder that are not on this device.
        var cloudBooks: [RemoteBook] {
            guard isSyncConfigured else { return [] }
            let here = Set(books.compactMap(\.remoteName))
            return remoteBooks.filter { !here.contains($0.name) }
        }

        func isInCloud(_ book: Book) -> Bool {
            isSyncConfigured && remoteBooks.contains { $0.name == book.remoteName }
        }
    }

    enum Alert: Equatable, Sendable {
        case deleteEverywhere(Book)
        case deleteRemote(RemoteBook)
    }

    enum Action: BindableAction {
        case task
        case addBookTapped
        case alert(PresentationAction<Alert>)
        case binding(BindingAction<State>)
        case bookTapped(Book)
        case deleteTapped(Book)
        case deleteEverywhereTapped(Book)
        case deleteRemoteTapped(RemoteBook)
        case dissolveTapped(BookGroup)
        case downloadFinished(RemoteBook, Result<Void, any Error>)
        case downloadTapped(RemoteBook)
        case failed(title: String, message: String)
        case filesPicked(Result<[URL], any Error>)
        case groupPicker(PresentationAction<GroupPickerFeature.Action>)
        case groupTapped(Book)
        case reader(PresentationAction<ReaderFeature.Action>)
        case settings(PresentationAction<SettingsFeature.Action>)
        case settingsTapped
        case subtitles(PresentationAction<SubtitlesFeature.Action>)
        case subtitlesTapped
        case syncFinished(Result<SyncReport, any Error>)
        case words(PresentationAction<WordsFeature.Action>)
        case wordsTapped
    }

    @Dependency(\.aiSettingsClient) var aiSettings
    @Dependency(\.groupClient) var groups
    @Dependency(\.libraryClient) var library
    @Dependency(\.syncClient) var sync
    @Dependency(\.syncSettingsClient) var syncSettings
    @Dependency(\.webSearchSettingsClient) var webSearchSettings

    var body: some ReducerOf<Self> {
        BindingReducer()
        Reduce { state, action in
            switch action {
            case .task:
                return syncIfConfigured(&state)

            case .addBookTapped:
                state.isImporterPresented = true
                return .none

            case let .bookTapped(book):
                state.reader = ReaderFeature.State(book: book)
                return .none

            case let .deleteTapped(book):
                return .run { send in
                    do { try await library.delete(book: book) }
                    catch { await send(.failed(title: "Couldn’t remove the book", message: error.localizedDescription)) }
                }

            case let .deleteEverywhereTapped(book):
                state.alert = .deleteEverywhere(book)
                return .none

            case let .alert(.presented(.deleteEverywhere(book))):
                return .run { _ in
                    // The file first: if it cannot go, the book stays as it was.
                    if let name = book.remoteName { try await sync.deleteRemote(name: name) }
                    try await library.delete(book: book)
                } catch: { error, send in
                    await send(.failed(title: "Couldn’t delete the book", message: error.localizedDescription))
                }

            case let .deleteRemoteTapped(remote):
                state.alert = .deleteRemote(remote)
                return .none

            case let .alert(.presented(.deleteRemote(remote))):
                return .run { _ in try await sync.deleteRemote(name: remote.name) } catch: { error, send in
                    await send(.failed(title: "Couldn’t delete the book", message: error.localizedDescription))
                }

            case let .downloadTapped(remote):
                guard state.downloading.insert(remote.name).inserted else { return .none }
                return .run { send in
                    await send(.downloadFinished(remote, Result { try await sync.download(name: remote.name) }))
                }

            case let .downloadFinished(remote, result):
                state.downloading.remove(remote.name)
                if case let .failure(error) = result {
                    return .send(.failed(title: "Couldn’t download “\(remote.title)”", message: error.localizedDescription))
                }
                // Where another device is in it, and its group, come with the document.
                return syncIfConfigured(&state)

            case let .groupTapped(book):
                state.groupPicker = GroupPickerFeature.State(book: book)
                return .none

            case .groupPicker(.presented(.delegate(.dismiss))):
                state.groupPicker = nil
                return .none

            case let .dissolveTapped(group):
                return .run { _ in try? await groups.dissolve(groupID: group.id) }

            case let .filesPicked(.success(urls)):
                return .run { send in
                    do {
                        for url in urls { try await library.add(file: url) }
                    } catch {
                        await send(.failed(title: "Couldn’t add the book", message: error.localizedDescription))
                    }
                }

            case let .filesPicked(.failure(error)):
                return .send(.failed(title: "Couldn’t add the book", message: error.localizedDescription))

            case let .failed(title, message):
                state.alert = AlertState {
                    TextState(title)
                } message: {
                    TextState(message)
                }
                return .none

            case let .reader(.presented(.delegate(.openBook(bookID, position)))):
                guard let book = state.books.first(where: { $0.id == bookID }) else { return .none }
                state.reader = ReaderFeature.State(book: book, jumpTo: position)
                return .none

            case .reader(.dismiss):
                // Closing a book is when its place is worth sending on.
                return syncIfConfigured(&state)

            case .settingsTapped:
                state.settings = SettingsFeature.State(
                    settings: aiSettings.load(),
                    sync: syncSettings.load(),
                    web: webSearchSettings.load()
                )
                return .none

            case .settings(.presented(.delegate(.dismiss))):
                state.settings = nil
                state.isSyncConfigured = sync.isConfigured()
                return .none

            case .settings(.dismiss):
                state.isSyncConfigured = sync.isConfigured()
                return .none

            case let .syncFinished(result):
                state.isSyncing = false
                if case let .failure(error) = result {
                    return .send(.failed(title: "Couldn’t sync", message: error.localizedDescription))
                }
                return .none

            case .subtitlesTapped:
                state.subtitles = SubtitlesFeature.State()
                return .none

            case .wordsTapped:
                state.words = WordsFeature.State()
                return .none

            case .words(.presented(.delegate(.dismiss))):
                state.words = nil
                return .none

            case .alert, .binding, .groupPicker, .reader, .settings, .subtitles, .words:
                return .none
            }
        }
        .ifLet(\.$alert, action: \.alert)
        .ifLet(\.$groupPicker, action: \.groupPicker) {
            GroupPickerFeature()
        }
        .ifLet(\.$reader, action: \.reader) {
            ReaderFeature()
        }
        .ifLet(\.$subtitles, action: \.subtitles) {
            SubtitlesFeature()
        }
        .ifLet(\.$settings, action: \.settings) {
            SettingsFeature()
        }
        .ifLet(\.$words, action: \.words) {
            WordsFeature()
        }
    }

    /// Quietly brings this device in line with the others, when a server
    /// has been set up.
    private func syncIfConfigured(_ state: inout State) -> Effect<Action> {
        state.isSyncConfigured = sync.isConfigured()
        guard state.isSyncConfigured, !state.isSyncing else { return .none }
        state.isSyncing = true
        return .run { send in
            await send(.syncFinished(Result { try await sync.sync() }))
        }
    }
}
