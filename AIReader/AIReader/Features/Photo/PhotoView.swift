#if os(iOS)
import ComposableArchitecture
import SwiftUI

/// The camera or the library, then the explanation of the word the finger
/// points at — or, with no finger, the photo's text to tap a word in — with
/// the next photo a tap away at the foot of the screen.
struct PhotoView: View {
    @Bindable var store: StoreOf<PhotoFeature>

    var body: some View {
        if let source = store.source {
            CameraView(source: source) { store.send(.photoTaken($0)) } onCancel: { store.send(.pickerCancelled) }
                .ignoresSafeArea()
        } else if !store.isChoosingWord, let lookup = store.scope(state: \.lookup, action: \.lookup.presented) {
            LookupView(store: lookup) {
                choosePhoto
                nextPhoto
            }
        } else {
            NavigationStack {
                content
                    .navigationTitle("Photo")
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbar {
                        ToolbarItemGroup(placement: .bottomBar) {
                            Button("Done") { store.send(.doneTapped) }
                            Spacer()
                            choosePhoto
                            nextPhoto
                        }
                    }
            }
            .sheet(item: $store.scope(state: \.lookup, action: \.lookup)) { lookup in
                LookupView(store: lookup)
            }
        }
    }

    private var choosePhoto: some View {
        Button("Choose photo", systemImage: "photo.on.rectangle") { store.send(.libraryTapped) }
    }

    private var nextPhoto: some View {
        Button("Next photo", systemImage: "camera") { store.send(.nextPhotoTapped) }
    }

    @ViewBuilder
    private var content: some View {
        if store.isReading {
            ProgressView("Reading the photo…")
        } else if let message = store.errorMessage {
            ContentUnavailableView("Nothing to read", systemImage: "text.viewfinder", description: Text(message))
        } else if let passage = store.passage {
            ScrollView {
                VStack(alignment: .leading, spacing: 12) {
                    Text("No finger pointed at a word. Tap one to explain it.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    TappableTextView(text: passage.text) { store.send(.wordTapped(utf16Offset: $0)) }
                }
                .padding()
            }
        } else {
            ContentUnavailableView(
                "Photo of a page",
                systemImage: "hand.point.up.left",
                description: Text("Photograph the page with a finger just below the word you mean.")
            )
        }
    }
}
#endif
