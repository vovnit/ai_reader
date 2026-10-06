import ComposableArchitecture
import SwiftUI

struct ContentsView: View {
    let store: StoreOf<ContentsFeature>

    var body: some View {
        ScrollViewReader { proxy in
            List(Array(store.entries.enumerated()), id: \.offset) { index, entry in
                Button { store.send(.entryTapped(index)) } label: {
                    HStack {
                        Text(entry.title)
                            .padding(.leading, CGFloat(entry.depth) * 16)
                        Spacer()
                        if index == store.current {
                            Image(systemName: "checkmark")
                                .accessibilityLabel("You are here")
                        }
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
            .overlay {
                if store.entries.isEmpty {
                    ContentUnavailableView("No contents", systemImage: "list.bullet", description: Text("This book has no table of contents."))
                }
            }
            .navigationTitle("Contents")
            .onAppear {
                if let current = store.current { proxy.scrollTo(current, anchor: .center) }
            }
        }
    }
}
