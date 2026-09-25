import ComposableArchitecture
import SwiftUI

/// The explanation panel for a tapped word: what it means here and what its
/// dictionary form is.
struct LookupView: View {
    @Bindable var store: StoreOf<LookupFeature>

    var body: some View {
        NavigationStack(path: $store.scope(state: \.path, action: \.path)) {
            ScrollView {
                VStack(alignment: .leading, spacing: 20) {
                    if let explanation = store.explanation {
                        content(explanation)
                    } else if let message = store.errorMessage {
                        Text(message).foregroundStyle(.secondary)
                    } else {
                        ProgressView().frame(maxWidth: .infinity)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding()
            }
            .navigationTitle(store.context.word)
            .toolbar {
                Button("Pronounce", systemImage: "speaker.wave.2") {
                    store.send(.speakTapped(store.context.word))
                }
                // A side panel, and a Mac sheet, have no edge to drag away by.
                Button("Done") { store.send(.doneTapped) }
                    .keyboardShortcut(.cancelAction)
            }
        } destination: { store in
            switch store.case {
            case let .entry(store): DictionaryEntryView(store: store)
            case let .xray(store): XRayView(store: store)
            case let .chat(store): ChatView(store: store)
            }
        }
        .presentationDetents([.medium, .large])
        .task { await store.send(.task).finish() }
    }

    @ViewBuilder
    private func content(_ explanation: WordExplanation) -> some View {
        Text(explanation.meaning)
            .font(.body)

        VStack(alignment: .leading, spacing: 4) {
            Text(explanation.lemma)
                .font(.headline)
            Text(explanation.formNote)
                .font(.subheadline)
                .foregroundStyle(.secondary)
        }

        actions

        if !store.context.sentence.isEmpty {
            Divider()
            VStack(alignment: .leading, spacing: 8) {
                Text(store.context.sentence)
                    .font(.callout)
                    .italic()
                    .foregroundStyle(.secondary)
                Button("Play sentence", systemImage: "play.circle") {
                    store.send(.speakTapped(store.context.sentence))
                }
                .font(.footnote)
                .buttonStyle(.plain)
                .foregroundStyle(.tint)
            }
        }

        if explanation.guessed || explanation.confidence < 0.6 {
            Divider()
            VStack(alignment: .leading, spacing: 4) {
                if explanation.guessed {
                    Label("Догадка, не из словаря", systemImage: "questionmark.circle")
                        .font(.footnote)
                        .foregroundStyle(.orange)
                }
                Text("Уверенность: \(Int((explanation.confidence * 100).rounded()))%")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
        }
    }

    /// The dictionary's own article, what the book makes of the word, and a
    /// conversation that starts from the explanation.
    private var actions: some View {
        HStack(spacing: 8) {
            // Only when the dictionary really has the lemma; a guess has no entry.
            if !store.entry.isEmpty {
                Button("Dictionary entry", systemImage: "book") { store.send(.entryTapped) }
            }
            if store.state.scope.corpus != nil {
                Button("X-ray", systemImage: "eye") { store.send(.xrayTapped) }
            }
            Button("Ask AI", systemImage: "bubble.left.and.text.bubble.right") { store.send(.askTapped) }
        }
        .buttonStyle(.bordered)
        .font(.footnote)
        .labelStyle(.titleOnly)
    }
}
