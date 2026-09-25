import ComposableArchitecture
import SwiftUI

struct SearchView: View {
    @Bindable var store: StoreOf<SearchFeature>

    var body: some View {
        List {
            if store.isSearching {
                HStack(spacing: 10) {
                    ProgressView()
                    Text("Searching…").foregroundStyle(.secondary)
                }
            } else if store.query.isEmpty {
                Text("Searches \(store.covers). Tap a result to go there.")
                    .foregroundStyle(.secondary)
            } else {
                Section {
                    ForEach(store.hits) { hit in
                        Button { store.send(.hitTapped(hit)) } label: {
                            SearchHitRow(hit: hit, showsBook: store.severalBooks)
                        }
                        .buttonStyle(.plain)
                    }
                } header: {
                    Text(caption)
                }
            }
            if let message = store.errorMessage {
                Text(message).font(.footnote).foregroundStyle(.secondary)
            }
        }
        .searchable(text: $store.draft, placement: searchPlacement, prompt: "Find in \(store.covers)")
        .onSubmit(of: .search) { store.send(.searchTapped) }
        .navigationTitle("Search")
    }

    /// The drawer keeps the field visible while scrolling; macOS has no drawer.
    private var searchPlacement: SearchFieldPlacement {
        #if os(iOS)
        .navigationBarDrawer(displayMode: .always)
        #else
        .automatic
        #endif
    }

    private var caption: String {
        let count = store.hits.count
        if count == 0 { return "Nothing found for “\(store.query)”." }
        let first = count >= SearchFeature.limit ? "First " : ""
        return "\(first)\(count) \(count == 1 ? "place" : "places") with “\(store.query)”"
    }
}

/// One hit: the excerpt with the match in bold, and where it is.
struct SearchHitRow: View {
    let hit: SearchHit
    let showsBook: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(excerpt)
            Text(showsBook ? "\(hit.bookTitle) · Chapter \(hit.chapter + 1)" : "Chapter \(hit.chapter + 1)")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .contentShape(Rectangle())
    }

    private var excerpt: AttributedString {
        let text = hit.excerpt as NSString
        let range = hit.matchRange.clamped(to: 0..<text.length)
        var before = AttributedString(text.substring(to: range.lowerBound))
        var match = AttributedString(text.substring(with: NSRange(range)))
        match.font = .body.bold()
        before.append(match)
        before.append(AttributedString(text.substring(from: range.upperBound)))
        return before
    }
}
