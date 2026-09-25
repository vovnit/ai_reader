import ComposableArchitecture
import SwiftUI
import UniformTypeIdentifiers

struct LibraryView: View {
    @Bindable var store: StoreOf<LibraryFeature>
    @Environment(\.openWindow) private var openWindow

    private let columns = [GridItem(.adaptive(minimum: 110), spacing: 16)]

    var body: some View {
        NavigationStack {
            Group {
                if store.books.isEmpty {
                    ContentUnavailableView(
                        "No books yet",
                        systemImage: "books.vertical",
                        description: Text("Add an EPUB to start reading.")
                    )
                } else {
                    shelf
                }
            }
            .navigationTitle("Library")
            .toolbar {
                if store.isSyncing {
                    ProgressView()
                }
                Button("Settings", systemImage: "gearshape") { store.send(.settingsTapped) }
                Button("Words", systemImage: "character.book.closed") { store.send(.wordsTapped) }
                #if os(macOS)
                Button("Subtitles", systemImage: "captions.bubble") { openWindow(id: SubtitlesView.windowID) }
                #else
                Button("Subtitles", systemImage: "captions.bubble") { store.send(.subtitlesTapped) }
                #endif
                Button("Add book", systemImage: "plus") { store.send(.addBookTapped) }
            }
            .fileImporter(
                isPresented: $store.isImporterPresented,
                allowedContentTypes: [.epub],
                allowsMultipleSelection: true
            ) { store.send(.filesPicked($0)) }
            .alert($store.scope(state: \.alert, action: \.alert))
            .sheet(item: $store.scope(state: \.groupPicker, action: \.groupPicker)) { picker in
                GroupPickerView(store: picker)
            }
            .sheet(item: $store.scope(state: \.settings, action: \.settings)) { settings in
                SettingsView(store: settings)
            }
            .sheet(item: $store.scope(state: \.words, action: \.words)) { words in
                WordsView(store: words)
            }
            .sheet(item: $store.scope(state: \.subtitles, action: \.subtitles)) { subtitles in
                NavigationStack {
                    SubtitlesView(store: subtitles)
                        .navigationTitle("Subtitles")
                        .toolbar {
                            Button("Done") { store.send(.subtitles(.dismiss)) }
                        }
                }
            }
            .navigationDestination(item: $store.scope(state: \.reader, action: \.reader)) { reader in
                ReaderView(store: reader)
            }
            .task { await store.send(.task).finish() }
        }
    }

    /// Each group under its name, then the books in none.
    private var shelf: some View {
        ScrollView {
            LazyVStack(alignment: .leading, spacing: 24) {
                ForEach(store.groups) { group in
                    let books = store.state.books(in: group.id)
                    if !books.isEmpty {
                        VStack(alignment: .leading, spacing: 12) {
                            heading(group, count: books.count)
                            grid(books)
                        }
                    }
                }
                grid(store.state.books(in: nil))
            }
            .padding()
        }
    }

    /// The line above a group's books: its name, and the way to dissolve it.
    private func heading(_ group: BookGroup, count: Int) -> some View {
        HStack {
            Text(group.name).font(.headline)
            Text("\(count) \(count == 1 ? "book" : "books")")
                .font(.subheadline)
                .foregroundStyle(.secondary)
            Spacer()
            Menu {
                Button("Dissolve group", systemImage: "rectangle.stack.badge.minus", role: .destructive) {
                    store.send(.dissolveTapped(group))
                }
            } label: {
                Image(systemName: "ellipsis.circle")
            }
        }
    }

    private func grid(_ books: [Book]) -> some View {
        LazyVGrid(columns: columns, spacing: 20) {
            ForEach(books) { book in
                Button {
                    store.send(.bookTapped(book))
                } label: {
                    VStack(alignment: .leading, spacing: 6) {
                        BookCoverView(book: book)
                        Text(book.title)
                            .font(.caption)
                            .lineLimit(2)
                        if let author = book.author {
                            Text(author)
                                .font(.caption2)
                                .foregroundStyle(.secondary)
                                .lineLimit(1)
                        }
                    }
                }
                .buttonStyle(.plain)
                .contextMenu {
                    Button("Group…", systemImage: "rectangle.stack") {
                        store.send(.groupTapped(book))
                    }
                    Button("Delete", systemImage: "trash", role: .destructive) {
                        store.send(.deleteTapped(book))
                    }
                }
            }
        }
    }
}
