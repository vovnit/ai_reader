import ComposableArchitecture
import Foundation

/// Reading one book: load its pages, remember the position, and look words up.
/// Holds the corpus — this book and its group — that searches run over.
@Reducer
struct ReaderFeature {
    @ObservableState
    struct State: Equatable, Identifiable {
        enum Document: Equatable {
            case loading
            case loaded(BookDocument)
            case failed(String)
        }

        /// A page the view is asked to turn to; a new id each time so the
        /// same offset can be asked for twice.
        struct Jump: Equatable {
            let id: UUID
            let offset: Int
        }

        @Shared(.readingStyle) var style
        var book: Book
        var document: Document = .loading
        /// The group's books, this one first, once the reader has them.
        var corpus: BookCorpus?
        /// What the group is called, when the book is in one.
        var groupName: String?
        /// A place to turn to as soon as the book is open, from a search hit
        /// in another book.
        var pendingJump: BookPosition?
        var jump: Jump?

        /// What the book is really in, preferring the prose over the metadata.
        var language: String? {
            guard case let .loaded(document) = document else { return book.language }
            return document.language ?? book.language
        }
        /// The text of the page on screen, so the menu's chat can be about it.
        var pageText = ""
        /// The end of the page on screen: how far the reader has got.
        var pageEnd = 0
        @Presents var lookup: LookupFeature.State?
        @Presents var menu: ReaderMenuFeature.State?

        var id: Int { book.id }

        /// The corpus with the position: what lookups and conversations search.
        var scope: ReadingScope {
            var chapter = 0
            if case let .loaded(document) = document { chapter = document.chapter(containing: pageEnd) }
            return ReadingScope(
                corpus: corpus,
                upTo: BookPosition(bookID: book.id, chapter: chapter, offset: pageEnd)
            )
        }

        init(book: Book, jumpTo: BookPosition? = nil) {
            self.book = book
            self.pendingJump = jumpTo
        }
    }

    enum Action {
        case task
        case groupLoaded(books: [Book], name: String?)
        case documentLoaded(BookDocument)
        case loadFailed(String)
        case menuTapped
        case menu(PresentationAction<ReaderMenuFeature.Action>)
        case pageChanged(offset: Int, text: String)
        case wordTapped(word: String, sentence: String)
        case lookupDismissed
        case lookup(PresentationAction<LookupFeature.Action>)
        case delegate(Delegate)

        enum Delegate: Equatable {
            /// A hit in another book of the group: open that one there.
            case openBook(Book.ID, at: BookPosition)
        }
    }

    @Dependency(\.dismiss) var dismiss
    @Dependency(\.libraryClient) var library
    @Dependency(\.uuid) var uuid

    var body: some ReducerOf<Self> {
        Reduce { state, action in
            switch action {
            case .task:
                guard case .loading = state.document else { return .none }
                let book = state.book
                return .run { send in
                    // The open book is searched first, then the rest of its group.
                    var books = [book]
                    var name: String?
                    if let groupID = book.groupID {
                        books += await library.inGroup(groupID: groupID).filter { $0.id != book.id }
                        name = await library.groupName(groupID: groupID)
                    }
                    await send(.groupLoaded(books: books, name: name))
                    do {
                        let document = try await BookDocumentLoader.load(
                            folder: book.folder,
                            packagePath: book.packagePath
                        )
                        await send(.documentLoaded(document))
                    } catch {
                        await send(.loadFailed(error.localizedDescription))
                    }
                }

            case let .groupLoaded(books, name):
                state.corpus = BookCorpus(books: books)
                state.groupName = name
                return .none

            case let .documentLoaded(document):
                state.document = .loaded(document)
                var effects: [Effect<Action>] = []
                if let corpus = state.corpus {
                    let id = state.book.id
                    effects.append(.run { _ in await corpus.provide(document, for: id) })
                }
                // A place that came from another device is found in this
                // rendering now that the text is here.
                if state.book.placeIsPending, let place = state.book.place {
                    let range = document.chapters.indices.contains(place.chapter)
                        ? document.chapters[place.chapter]
                        : NSRange(location: 0, length: 0)
                    state.book.readingOffset = range.location + place.resolve(in: document.chapterText(place.chapter))
                    state.book.placeIsPending = false
                }
                if let jump = state.pendingJump {
                    state.pendingJump = nil
                    state.book.readingOffset = jump.offset
                }
                if let detected = document.language, detected != state.book.language {
                    state.book.language = detected
                    let id = state.book.id
                    effects.append(.run { _ in try await library.saveLanguage(bookID: id, language: detected) })
                }
                return .merge(effects)

            case let .loadFailed(message):
                state.document = .failed(message)
                return .none

            case let .pageChanged(offset, text):
                state.pageText = text
                state.pageEnd = offset + (text as NSString).length
                guard state.book.readingOffset != offset || state.book.place == nil else { return .none }
                state.book.readingOffset = offset
                var place: ReadingPlace?
                if case let .loaded(document) = state.document {
                    let chapter = document.chapter(containing: offset)
                    place = ReadingPlace(
                        chapter: chapter,
                        chapterText: document.chapterText(chapter),
                        offset: offset - document.chapters[chapter].location
                    )
                }
                state.book.place = place
                let id = state.book.id
                return .run { _ in try await library.saveReadingOffset(bookID: id, offset: offset, place: place) }

            case .menuTapped:
                let count = state.corpus?.books.count ?? 1
                state.menu = ReaderMenuFeature.State(
                    bookID: state.book.id,
                    page: state.pageText,
                    scope: state.scope,
                    covers: state.groupName.map { "the \(count) books of “\($0)”" } ?? "this book"
                )
                return .none

            case .menu(.presented(.delegate(.closeBook))):
                state.menu = nil
                return .run { _ in await dismiss() }

            case .menu(.presented(.delegate(.dismiss))):
                state.menu = nil
                return .none

            case let .menu(.presented(.delegate(.jump(position)))):
                state.menu = nil
                return jump(to: position, &state)

            case let .lookup(.presented(.delegate(.jump(position)))):
                state.lookup = nil
                return jump(to: position, &state)

            case let .wordTapped(word, sentence):
                state.lookup = LookupFeature.State(
                    id: uuid(),
                    context: LookupContext(
                        word: word,
                        sentence: sentence,
                        language: state.language,
                        bookID: state.book.id
                    ),
                    scope: state.scope
                )
                return .none

            case .lookupDismissed:
                state.lookup = nil
                return .none

            case .lookup, .menu, .delegate:
                return .none
            }
        }
        .ifLet(\.$lookup, action: \.lookup) {
            LookupFeature()
        }
        .ifLet(\.$menu, action: \.menu) {
            ReaderMenuFeature()
        }
    }

    /// Turns to a place in this book, or asks for the other book to be opened
    /// there.
    private func jump(to position: BookPosition, _ state: inout State) -> Effect<Action> {
        guard position.bookID == state.book.id else {
            return .send(.delegate(.openBook(position.bookID, at: position)))
        }
        state.jump = State.Jump(id: uuid(), offset: position.offset)
        return .none
    }
}
