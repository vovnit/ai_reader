import ComposableArchitecture
import Foundation

/// A book's offline glossary: count the book's words, then ask the model
/// about those the glossary lacks, a batch at a time, filing each answer as it
/// arrives — so a stopped or failed run keeps what it paid for, and the next
/// asks only about the rest. Lookups use the glossary while it fills.
@Reducer
struct GlossaryFeature {
    /// Requests in flight at once.
    private static let parallel = 4

    @ObservableState
    struct State: Equatable {
        let bookID: Book.ID
        let title: String
        /// The book's text comes from here.
        let scope: ReadingScope
        var isCounted = false
        var total = 0
        var defined = 0
        var model = ""
        var isRunning = false
        var isStopping = false
        var errorMessage: String?
        var missing: [BookWord] = []
        var pending: [[BookWord]] = []
        var inFlight = 0

        /// The dictionary the glossary is.
        var name: String { GlossaryClient.name(forTitle: title) }
        /// What defining the missing words should cost, roughly.
        var estimatedTokens: Int { missing.count * GlossaryPrompt.tokensPerWord }
    }

    enum Action {
        case task
        case counted(total: Int, missing: [BookWord], model: String)
        case startTapped
        case stopTapped
        case batchDefined(Set<String>)
        case batchFailed(String)
    }

    @Dependency(\.aiSettingsClient) var aiSettings
    @Dependency(\.glossaryClient) var glossary

    var body: some ReducerOf<Self> {
        Reduce { state, action in
            switch action {
            case .task:
                guard !state.isCounted else { return .none }
                return .run { [bookID = state.bookID, title = state.title, corpus = state.scope.corpus] send in
                    var chapters: [String] = []
                    if let document = await corpus?.document(for: bookID) {
                        chapters = document.chapters.indices.map(document.chapterText)
                    }
                    let words = BookWords.collect(chapters)
                    let defined = await glossary.definedForms(title)
                    let missing = words.filter { !defined.contains($0.form) }
                    await send(.counted(total: words.count, missing: missing, model: aiSettings.load().model))
                }

            case let .counted(total, missing, model):
                state.total = total
                state.missing = missing
                state.defined = total - missing.count
                state.model = model
                state.isCounted = true
                return .none

            case .startTapped:
                guard !state.isRunning, !state.missing.isEmpty else { return .none }
                state.isRunning = true
                state.isStopping = false
                state.errorMessage = nil
                state.pending = stride(from: 0, to: state.missing.count, by: GlossaryPrompt.batchSize).map {
                    Array(state.missing[$0..<min($0 + GlossaryPrompt.batchSize, state.missing.count)])
                }
                var effects: [Effect<Action>] = []
                for _ in 0..<Self.parallel { effects.append(nextBatch(&state)) }
                return .merge(effects)

            case .stopTapped:
                // The requests in flight finish; no more are made.
                state.isStopping = true
                return .none

            case let .batchDefined(forms):
                state.inFlight -= 1
                state.missing.removeAll { forms.contains($0.form) }
                state.defined += forms.count
                return nextBatch(&state)

            case let .batchFailed(message):
                // The rest would most likely fail the same way.
                state.inFlight -= 1
                state.errorMessage = message
                state.isStopping = true
                return nextBatch(&state)
            }
        }
    }

    private func nextBatch(_ state: inout State) -> Effect<Action> {
        guard !state.isStopping, !state.pending.isEmpty else {
            if state.inFlight == 0 { state.isRunning = false }
            return .none
        }
        let batch = state.pending.removeFirst()
        state.inFlight += 1
        return .run { [title = state.title] send in
            do {
                let definitions = try await glossary.define(batch)
                try await glossary.add(definitions, title)
                await send(.batchDefined(Set(definitions.keys)))
            } catch {
                await send(.batchFailed(error.localizedDescription))
            }
        }
    }
}
