#if os(iOS)
import ComposableArchitecture
import SwiftUI

/// The camera, then the explanation of the word the finger points at, with
/// the next photo a tap away at the foot of the screen.
struct PhotoView: View {
    @Bindable var store: StoreOf<PhotoFeature>

    var body: some View {
        if store.isTakingPhoto {
            CameraView { store.send(.photoTaken($0)) } onCancel: { store.send(.cameraCancelled) }
                .ignoresSafeArea()
        } else if let lookup = store.scope(state: \.lookup, action: \.lookup.presented) {
            LookupView(store: lookup) {
                nextPhoto
            }
        } else {
            NavigationStack {
                status
                    .navigationTitle("Photo")
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbar {
                        ToolbarItemGroup(placement: .bottomBar) {
                            Button("Done") { store.send(.doneTapped) }
                            Spacer()
                            nextPhoto
                        }
                    }
            }
        }
    }

    private var nextPhoto: some View {
        Button("Next photo", systemImage: "camera") { store.send(.nextPhotoTapped) }
    }

    @ViewBuilder
    private var status: some View {
        if store.isReading {
            ProgressView("Reading the photo…")
        } else if let message = store.errorMessage {
            ContentUnavailableView("Nothing to read", systemImage: "text.viewfinder", description: Text(message))
        } else {
            ContentUnavailableView(
                "No word pointed at",
                systemImage: "hand.point.up.left",
                description: Text("Hold a finger just below the word you mean, and take the photo again.")
            )
        }
    }
}
#endif
