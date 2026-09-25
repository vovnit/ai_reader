import ComposableArchitecture
import SwiftUI
import UniformTypeIdentifiers

struct WordsView: View {
    @Bindable var store: StoreOf<WordsFeature>
    /// False when pushed onto a navigation stack that already has a way back.
    var showsDoneButton = true

    var body: some View {
        if showsDoneButton {
            NavigationStack { words }
        } else {
            words
        }
    }

    private var words: some View {
        Group {
            if store.lookups.isEmpty {
                ContentUnavailableView(
                    "No words yet",
                    systemImage: "character.book.closed",
                    description: Text("Words you look up while reading collect here.")
                )
            } else {
                list
            }
        }
        .navigationTitle(showsDoneButton ? "Words" : "Lookups")
        .toolbar {
            if showsDoneButton {
                Button("Done") { store.send(.doneTapped) }
            }
            if !store.lookups.isEmpty {
                Button("Export for Anki", systemImage: "square.and.arrow.up") { store.send(.exportTapped) }
                Button("Practice", systemImage: "rectangle.on.rectangle.angled") { store.send(.practiceTapped) }
            }
        }
        .navigationDestination(item: $store.scope(state: \.match, action: \.match)) { match in
            MatchView(store: match)
        }
        .fileExporter(
            isPresented: Binding(get: { store.export != nil }, set: { if !$0 { store.export = nil } }),
            document: store.export,
            contentType: .plainText,
            defaultFilename: "anki-cards"
        ) { _ in }
    }

    private var list: some View {
        List {
            ForEach(store.lookups) { lookup in
                VStack(alignment: .leading, spacing: 4) {
                    HStack(alignment: .firstTextBaseline) {
                        Text(lookup.word)
                            .font(.headline)
                        if lookup.lemma != lookup.word, !lookup.lemma.isEmpty {
                            Text(lookup.lemma)
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                        }
                        Spacer()
                        Button("Pronounce", systemImage: "speaker.wave.2") {
                            store.send(.speakTapped(lookup))
                        }
                        .labelStyle(.iconOnly)
                        .buttonStyle(.plain)
                        .foregroundStyle(.tint)
                    }
                    Text(lookup.meaning)
                        .font(.subheadline)
                    if !lookup.sentence.isEmpty {
                        Text(lookup.sentence)
                            .font(.caption)
                            .italic()
                            .foregroundStyle(.secondary)
                            .lineLimit(2)
                    }
                }
                .padding(.vertical, 2)
                .swipeActions {
                    Button("Delete", systemImage: "trash", role: .destructive) {
                        store.send(.deleteTapped(lookup))
                    }
                }
            }
        }
        .listStyle(.plain)
    }
}
