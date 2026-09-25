import ComposableArchitecture
import SwiftUI

/// The shared passage with a word to pick, and the explanation over it — the
/// same panel the reader sees in a book.
struct ExplainView: View {
    @Bindable var store: StoreOf<ExplainFeature>

    var body: some View {
        NavigationStack {
            ScrollView {
                TappableTextView(text: store.passage.text) { store.send(.wordTapped(utf16Offset: $0)) }
                    .padding()
            }
            .navigationTitle("Explain")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                Button("Done") { store.send(.doneTapped) }
            }
            .safeAreaInset(edge: .bottom) {
                if store.passage.word == nil {
                    Text("Tap a word to explain it")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                        .padding(.bottom)
                }
            }
        }
        .sheet(item: $store.scope(state: \.$lookup, action: \.lookup)) { lookup in
            LookupView(store: lookup)
        }
        .task { await store.send(.task).finish() }
    }
}
