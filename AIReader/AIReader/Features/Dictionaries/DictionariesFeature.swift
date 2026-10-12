import ComposableArchitecture
import Foundation
import SQLiteData

/// The dictionaries the app searches: the bundled one, unless the reader
/// removed it, plus any they added. Anything that is not already a pack is
/// converted into one on the way in, which for a large dictionary takes a
/// moment.
@Reducer
struct DictionariesFeature {
    @ObservableState
    struct State: Equatable {
        @FetchAll(DictionaryPack.order { $0.addedAt })
        var packs

        var isImporterPresented = false
        var isImporting = false
        @Presents var alert: AlertState<Never>?

        var hasBundledPack: Bool { packs.contains { $0.isBundled } }
    }

    enum Action: BindableAction {
        case addTapped
        case alert(PresentationAction<Never>)
        case binding(BindingAction<State>)
        case deleteTapped(DictionaryPack)
        case failed(String)
        case filesPicked(Result<[URL], any Error>)
        case imported
        case restoreTapped
        case toggled(DictionaryPack)
    }

    @Dependency(\.dictionaryPacksClient) var packs

    var body: some ReducerOf<Self> {
        BindingReducer()
        Reduce { state, action in
            switch action {
            case .addTapped:
                state.isImporterPresented = true
                return .none

            case let .filesPicked(.success(urls)):
                state.isImporting = true
                return .run { send in
                    do {
                        try await packs.add(files: urls)
                        await send(.imported)
                    } catch {
                        await send(.failed(error.localizedDescription))
                    }
                }

            case let .filesPicked(.failure(error)):
                return .send(.failed(error.localizedDescription))

            case let .deleteTapped(pack):
                return .run { _ in await packs.remove(pack: pack) }

            case .restoreTapped:
                return .run { _ in await packs.restoreBundled() }

            case let .toggled(pack):
                return .run { _ in await packs.setEnabled(pack: pack, isEnabled: !pack.isEnabled) }

            case .imported:
                state.isImporting = false
                return .none

            case let .failed(message):
                state.isImporting = false
                state.alert = AlertState {
                    TextState("Couldn’t add the dictionary")
                } message: {
                    TextState(message)
                }
                return .none

            case .alert, .binding:
                return .none
            }
        }
        .ifLet(\.$alert, action: \.alert)
    }
}

/// Adding, enabling and removing dictionaries, and bringing back the bundled
/// one once removed.
@DependencyClient
struct DictionaryPacksClient: Sendable {
    var add: @Sendable (_ files: [URL]) async throws -> Void
    var remove: @Sendable (_ pack: DictionaryPack) async -> Void
    var restoreBundled: @Sendable () async -> Void
    var setEnabled: @Sendable (_ pack: DictionaryPack, _ isEnabled: Bool) async -> Void
}

extension DictionaryPacksClient: DependencyKey {
    static var liveValue: Self {
        Self(
            add: { urls in
                @Dependency(\.defaultDatabase) var database
                let drafts = try DictionaryPackImporter.add(from: urls)
                try await database.write { db in
                    for draft in drafts { try DictionaryPack.insert { draft }.execute(db) }
                }
            },
            remove: { pack in
                @Dependency(\.defaultDatabase) var database
                try? await database.write { db in
                    try DictionaryPack.delete().where { $0.id.eq(pack.id) }.execute(db)
                }
                // The bundled pack's file is part of the app; only its record goes.
                guard !pack.isBundled, let url = pack.url else { return }
                try? FileManager.default.removeItem(at: url)
            },
            restoreBundled: {
                @Dependency(\.defaultDatabase) var database
                try? await database.write { db in
                    // A second tap may arrive before the list shows the first.
                    let bundled = try DictionaryPack.where { $0.fileName.is(nil) }.fetchOne(db)
                    guard bundled == nil else { return }
                    // The same row the "Add dictionaries" migration seeds.
                    try DictionaryPack.insert {
                        DictionaryPack.Draft(
                            name: "Bundled dictionary",
                            fileName: nil,
                            targetLanguage: "fr",
                            definitionLanguage: "ru"
                        )
                    }
                    .execute(db)
                }
            },
            setEnabled: { pack, isEnabled in
                @Dependency(\.defaultDatabase) var database
                try? await database.write { db in
                    try DictionaryPack
                        .update { $0.isEnabled = isEnabled }
                        .where { $0.id.eq(pack.id) }
                        .execute(db)
                }
            }
        )
    }

    static let testValue = Self()
}

extension DependencyValues {
    var dictionaryPacksClient: DictionaryPacksClient {
        get { self[DictionaryPacksClient.self] }
        set { self[DictionaryPacksClient.self] = newValue }
    }
}
