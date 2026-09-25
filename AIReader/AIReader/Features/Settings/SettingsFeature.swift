import ComposableArchitecture
import Foundation

/// Where lookups are sent: the endpoint, the token, and which of the models it
/// offers to use. How the model reaches the web. And where the app's state is
/// shared between devices. Changes are saved as they are made.
@Reducer
struct SettingsFeature {
    @ObservableState
    struct State: Equatable {
        var settings: AISettings
        var sync: SyncSettings
        var web: WebSearchSettings
        var models: [String] = []
        var isLoadingModels = false
        var isSyncing = false
        var errorMessage: String?
        var syncMessage: String?
        @Presents var dictionaries: DictionariesFeature.State?

        /// The models to choose from, always including the current one so the
        /// selection is never lost.
        var selectableModels: [String] {
            models.contains(settings.model) || settings.model.isEmpty
                ? models
                : ([settings.model] + models)
        }
    }

    enum Action: BindableAction {
        case binding(BindingAction<State>)
        case dictionaries(PresentationAction<DictionariesFeature.Action>)
        case dictionariesTapped
        case doneTapped
        case loadModelsTapped
        case modelsLoaded([String])
        case modelsFailed(String)
        case syncTapped
        case syncFinished(Result<SyncReport, any Error>)
        case delegate(Delegate)

        enum Delegate {
            case dismiss
        }
    }

    @Dependency(\.aiSettingsClient) var aiSettings
    @Dependency(\.syncClient) var syncClient
    @Dependency(\.syncSettingsClient) var syncSettings
    @Dependency(\.webSearchSettingsClient) var webSearchSettings

    var body: some ReducerOf<Self> {
        // Both reducers are combined before `onChange` so that edits made
        // through a binding are seen too; attached to `Reduce` alone, `onChange`
        // reads its "old" value after `BindingReducer` has already written.
        CombineReducers {
            BindingReducer()
            Reduce { state, action in
                switch action {
                case .doneTapped:
                    return .send(.delegate(.dismiss))

                case .loadModelsTapped:
                    state.isLoadingModels = true
                    state.errorMessage = nil
                    let settings = state.settings
                    return .run { send in
                        do {
                            await send(.modelsLoaded(try await ChatAPI.models(settings: settings)))
                        } catch {
                            await send(.modelsFailed(error.localizedDescription))
                        }
                    }

                case let .modelsLoaded(models):
                    state.isLoadingModels = false
                    state.models = models
                    if !models.contains(state.settings.model), let first = models.first {
                        state.settings.model = first
                    }
                    return .none

                case let .modelsFailed(message):
                    state.isLoadingModels = false
                    state.models = []
                    state.errorMessage = message
                    return .none

                case .dictionariesTapped:
                    state.dictionaries = DictionariesFeature.State()
                    return .none

                case .syncTapped:
                    guard !state.isSyncing else { return .none }
                    state.isSyncing = true
                    state.syncMessage = nil
                    return .run { send in
                        await send(.syncFinished(Result { try await syncClient.sync() }))
                    }

                case let .syncFinished(result):
                    state.isSyncing = false
                    switch result {
                    case let .success(report): state.syncMessage = report.summary
                    case let .failure(error): state.syncMessage = error.localizedDescription
                    }
                    return .none

                case .binding, .delegate, .dictionaries:
                    return .none
                }
            }
        }
        .onChange(of: \.settings) { _, settings in
            Reduce { _, _ in
                aiSettings.save(settings: settings)
                return .none
            }
        }
        .onChange(of: \.sync) { _, sync in
            Reduce { _, _ in
                syncSettings.save(settings: sync)
                return .none
            }
        }
        .onChange(of: \.web) { _, web in
            Reduce { _, _ in
                webSearchSettings.save(settings: web)
                return .none
            }
        }
        .ifLet(\.$dictionaries, action: \.dictionaries) {
            DictionariesFeature()
        }
        // A model list belongs to the endpoint it came from.
        .onChange(of: \.settings.endpoint) { _, _ in
            Reduce { state, _ in
                state.models = []
                state.errorMessage = nil
                return .none
            }
        }
    }
}
