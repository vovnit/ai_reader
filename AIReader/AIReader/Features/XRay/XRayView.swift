import ComposableArchitecture
import SwiftUI

struct XRayView: View {
    @Bindable var store: StoreOf<XRayFeature>

    var body: some View {
        Group {
            if store.isAsking {
                asking
            } else {
                answer
            }
        }
        .navigationTitle(store.isAsking ? "X-ray" : store.term)
        .task { await store.send(.task).finish() }
    }

    /// The term is typed here when nothing was tapped.
    private var asking: some View {
        Form {
            Section {
                TextField("A name, a place, a word", text: $store.draft)
                    .autocorrectionDisabled()
                    .onSubmit { store.send(.termSubmitted) }
                Button("X-ray") { store.send(.termSubmitted) }
                    .disabled(store.draft.trimmingCharacters(in: .whitespaces).isEmpty)
            } footer: {
                Text("A name, a place, a word the book uses its own way: what the book has said about it so far.")
            }
        }
    }

    private var answer: some View {
        List {
            Section {
                if store.isWorking {
                    HStack(spacing: 10) {
                        ProgressView()
                        Text("Reading the book…").foregroundStyle(.secondary)
                    }
                } else if !store.answer.isEmpty {
                    Text(store.answer).textSelection(.enabled)
                    Button("Ask AI", systemImage: "bubble.left.and.text.bubble.right") {
                        store.send(.askTapped)
                    }
                }
                if let message = store.errorMessage {
                    Text(message).font(.footnote).foregroundStyle(.secondary)
                }
            }

            if !store.isWorking {
                Section {
                    ForEach(store.passages) { hit in
                        Button { store.send(.hitTapped(hit)) } label: {
                            SearchHitRow(hit: hit, showsBook: store.severalBooks)
                        }
                        .buttonStyle(.plain)
                    }
                } header: {
                    Text(store.passages.isEmpty
                         ? "Not met yet in what has been read."
                         : "Where it has appeared so far — tap to go there.")
                }
            }
        }
    }
}
