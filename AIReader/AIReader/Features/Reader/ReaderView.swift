import ComposableArchitecture
import SwiftUI

struct ReaderView: View {
    @Bindable var store: StoreOf<ReaderFeature>
    #if os(iOS)
    @Environment(\.horizontalSizeClass) private var sizeClass
    #endif

    var body: some View {
        Group {
            switch store.document {
            case .loading:
                ProgressView()

            case let .failed(message):
                ContentUnavailableView(
                    "Couldn’t open the book",
                    systemImage: "book.closed",
                    description: Text(message)
                )

            case let .loaded(document):
                BookPagesView(
                    document: document,
                    style: store.style,
                    startingOffset: store.book.readingOffset,
                    jump: store.jump,
                    onPageChanged: { store.send(.pageChanged(offset: $0, text: $1)) },
                    onWordTapped: { store.send(.wordTapped(word: $0.word, sentence: $0.sentence)) }
                )
            }
        }
        .navigationTitle(store.book.title)
        #if os(iOS)
        .toolbar(.hidden, for: .navigationBar)
        .overlay(alignment: .bottom) {
            if store.lookup == nil {
                MenuHandle { store.send(.menuTapped) }
            }
        }
        #else
        .toolbar {
            Button("Menu", systemImage: "list.bullet") { store.send(.menuTapped) }
        }
        #endif
        .modifier(LookupPanel(store: store, asSheet: isCompact))
        .sheet(item: $store.scope(state: \.$menu, action: \.menu)) { menu in
            ReaderMenuView(store: menu)
        }
        .task { await store.send(.task).finish() }
    }

    private var isCompact: Bool {
        #if os(iOS)
        sizeClass == .compact
        #else
        false
        #endif
    }
}

/// A side panel where there is room, so the passage stays in view while the
/// word is explained; a half-height sheet on a phone. The phone must not carry
/// an inspector at all: even closed, one pops the reader off the navigation
/// stack there, and standing in for a sheet it ignores the detents and goes
/// blank when a drag clears the lookup before it closes.
private struct LookupPanel: ViewModifier {
    @Bindable var store: StoreOf<ReaderFeature>
    let asSheet: Bool

    func body(content: Content) -> some View {
        if asSheet {
            content.sheet(item: $store.scope(state: \.$lookup, action: \.lookup)) { lookup in
                LookupView(store: lookup)
            }
        } else {
            content.inspector(isPresented: Binding(
                get: { store.lookup != nil },
                set: { if !$0 { store.send(.lookup(.dismiss)) } }
            )) {
                if let lookup = store.scope(state: \.lookup, action: \.lookup.presented) {
                    LookupView(store: lookup)
                }
            }
        }
    }
}
