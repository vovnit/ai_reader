import ComposableArchitecture
import SwiftUI

/// A book's offline glossary: how many of its words have a definition, what
/// defining the rest should cost, and the button that does it.
struct GlossaryView: View {
    let store: StoreOf<GlossaryFeature>

    var body: some View {
        Form {
            Section {
                Text("The model is asked, once, what each word of this book means where it stands. Its answers become the dictionary “\(store.name)”, so lookups in this book work without a network.")
            }
            Section {
                if !store.isCounted {
                    Text("Counting words…").foregroundStyle(.secondary)
                } else {
                    Text(status)
                    if let message = store.errorMessage {
                        Text(message).foregroundStyle(.secondary)
                    }
                    if store.isRunning {
                        Button("Stop") { store.send(.stopTapped) }
                            .disabled(store.isStopping)
                    } else if store.defined < store.total {
                        Button(store.defined > 0 ? "Continue" : "Write glossary") { store.send(.startTapped) }
                    }
                }
            }
        }
        .navigationTitle("Offline glossary")
        .task { await store.send(.task).finish() }
    }

    private var status: String {
        let defined = store.defined.formatted()
        let total = store.total.formatted()
        if store.isRunning { return "Defined \(defined) of \(total) words." }
        if store.defined == store.total { return "All \(total) words are defined." }
        let done = store.defined > 0 ? "\(defined) of \(total) words are defined; the rest" : "\(total) words"
        return "\(done) should take about \(store.estimatedTokens.formatted()) tokens with \(store.model)."
    }
}
