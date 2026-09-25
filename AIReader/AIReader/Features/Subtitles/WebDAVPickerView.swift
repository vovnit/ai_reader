import ComposableArchitecture
import SwiftUI

/// One folder of the server: folders to enter, subtitle files to pick.
struct WebDAVPickerView: View {
    let store: StoreOf<WebDAVPickerFeature>

    var body: some View {
        NavigationStack {
            List {
                if let message = store.errorMessage {
                    Text(message).foregroundStyle(.secondary)
                }
                ForEach(store.entries) { entry in
                    Button {
                        store.send(.entryTapped(entry))
                    } label: {
                        Label(entry.name, systemImage: icon(for: entry))
                    }
                    .disabled(!entry.isFolder && !isSubtitle(entry))
                }
            }
            .overlay {
                if store.isLoading { ProgressView() }
            }
            .navigationTitle(store.title)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { store.send(.cancelTapped) }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button("Up", systemImage: "arrow.up") { store.send(.upTapped) }
                        .disabled(store.folder.path == "/" || store.folder.path.isEmpty)
                }
            }
        }
        .task { await store.send(.task).finish() }
    }

    private func icon(for entry: WebDAVEntry) -> String {
        entry.isFolder ? "folder" : isSubtitle(entry) ? "captions.bubble" : "doc"
    }

    private func isSubtitle(_ entry: WebDAVEntry) -> Bool {
        SubtitleClient.extensions.contains(entry.url.pathExtension.lowercased())
    }
}
