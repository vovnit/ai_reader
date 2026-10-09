#if os(iOS)
import ComposableArchitecture
import SwiftUI

/// The camera, then the text read off the photo with a word to tap, and the
/// explanation of the word pointed at over it.
struct PhotoView: View {
    @Bindable var store: StoreOf<PhotoFeature>

    var body: some View {
        if store.isTakingPhoto {
            CameraView { store.send(.photoTaken($0)) } onCancel: { store.send(.cameraCancelled) }
                .ignoresSafeArea()
        } else {
            NavigationStack {
                content
                    .navigationTitle("Photo")
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbar {
                        ToolbarItem(placement: .cancellationAction) {
                            Button("Done") { store.send(.doneTapped) }
                        }
                        ToolbarItem(placement: .primaryAction) {
                            Button("Retake", systemImage: "camera") { store.send(.retakeTapped) }
                        }
                    }
            }
            .sheet(item: $store.scope(state: \.$lookup, action: \.lookup)) { lookup in
                LookupView(store: lookup)
            }
        }
    }

    @ViewBuilder
    private var content: some View {
        if store.isReading {
            ProgressView("Reading the photo…")
        } else if let message = store.errorMessage {
            ContentUnavailableView("Nothing to read", systemImage: "text.viewfinder", description: Text(message))
        } else {
            ScrollView {
                WordFlowView(chunks: store.chunks) { store.send(.wordTapped(utf16Offset: $0)) }
                    .font(.title3)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding()
            }
            .safeAreaInset(edge: .bottom) {
                Text(store.passage?.pointedOffset == nil
                    ? "No finger was seen pointing at a word. Tap one to explain it."
                    : "Tap a word to explain it")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .padding(.bottom)
            }
        }
    }
}
#endif
