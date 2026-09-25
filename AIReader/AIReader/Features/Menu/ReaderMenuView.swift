import ComposableArchitecture
import SwiftUI

struct ReaderMenuView: View {
    @Bindable var store: StoreOf<ReaderMenuFeature>

    var body: some View {
        NavigationStack(path: $store.scope(state: \.path, action: \.path)) {
            List {
                Button {
                    store.send(.wordsTapped)
                } label: {
                    Label("Lookups", systemImage: "character.book.closed")
                }
                Button {
                    store.send(.searchTapped)
                } label: {
                    Label("Search", systemImage: "magnifyingglass")
                }
                Button {
                    store.send(.xrayTapped)
                } label: {
                    Label("X-ray", systemImage: "eye")
                }
                Button {
                    store.send(.chatTapped)
                } label: {
                    Label("Ask about this page", systemImage: "bubble.left.and.text.bubble.right")
                }
                Button {
                    store.send(.displayTapped)
                } label: {
                    Label("Display", systemImage: "textformat.size")
                }
                Button {
                    store.send(.closeBookTapped)
                } label: {
                    Label("Close book", systemImage: "books.vertical")
                }
            }
            .navigationTitle("Menu")
            .toolbar {
                Button("Done") { store.send(.doneTapped) }
            }
        } destination: { store in
            switch store.case {
            case let .chat(store): ChatView(store: store)
            case let .display(store): DisplaySettingsView(store: store)
            case let .search(store): SearchView(store: store)
            case let .words(store): WordsView(store: store, showsDoneButton: false)
            case let .xray(store): XRayView(store: store)
            }
        }
    }
}

/// The control that summons the menu: a small handle at the foot of the page,
/// tapped or pulled up like the sheet it opens.
struct MenuHandle: View {
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Capsule()
                .fill(.secondary)
                .frame(width: 44, height: 5)
                .padding(.vertical, 12)
                .padding(.horizontal, 40)
                .contentShape(Capsule())
        }
        .buttonStyle(.plain)
        // A drag cancels the button's tap, so the two never both fire.
        .simultaneousGesture(
            DragGesture(minimumDistance: 10).onEnded { drag in
                if drag.translation.height < -20 { action() }
            }
        )
        .accessibilityLabel("Menu")
    }
}
