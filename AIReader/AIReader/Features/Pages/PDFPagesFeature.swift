import ComposableArchitecture
import Foundation

/// A PDF book's pages as the PDF draws them — a figure, a table, the page as
/// printed — with a tapped word explained as on the reflowed text. Opened
/// from the reader's menu for a book made from a PDF on this device, at the
/// page as far through the PDF as the reader is through the book, or where
/// the page view was last left. The page's text runs together as a
/// photographed page's does (`PagePassage`), so the sentence around a word
/// is found across line breaks. Every word a tap finds can be marked on the
/// page, as the reader chooses. The same as the web app's `PagesFeature`;
/// the zoom is PDFKit's own.
@Reducer
struct PDFPagesFeature {
    @ObservableState
    struct State: Equatable {
        let pdf: URL
        let bookID: Book.ID
        let language: String?
        /// What a lookup may search: the book up to where the reader is.
        let scope: ReadingScope
        /// How far through the book the reader is, from 0 to 1, where the
        /// pages first open; nil when they reopen at `page`.
        var startFraction: Double?
        /// The page on screen, from 0.
        var page: Int
        /// Each page's text, once the PDF is open.
        var texts: [String] = []
        var errorMessage: String?
        /// The characters of the page on screen the word looked up covers,
        /// marked while its lookup is open.
        var marked: NSRange?
        /// Whether every word a tap finds is marked on the page; kept between openings.
        @Shared(.appStorage("pdfPagesShowsWords")) var showsWords = false
        /// The characters of the page on screen each of its words covers, while they are marked.
        var words: [NSRange] = []
        @Presents var lookup: LookupFeature.State?

        var pageCount: Int { texts.count }

        /// The pages of the book open in `reader`; nil when its PDF is not on this device.
        init?(reader: ReaderFeature.State) {
            guard reader.hasPages, case let .loaded(document) = reader.document else { return nil }
            pdf = BookStorage.originalPDF(inFolder: reader.book.folder)
            bookID = reader.book.id
            language = reader.language
            scope = reader.scope
            startFraction = reader.pdfPage == nil
                ? Double(reader.book.readingOffset) / Double(max(document.text.length, 1))
                : nil
            page = reader.pdfPage ?? 0
        }
    }

    enum Action {
        case task
        case opened(Result<[String], any Error>)
        case pageShown(Int)
        case previousTapped
        case nextTapped
        /// A character of the page on screen, as an index into its text.
        case characterTapped(Int)
        case showsWordsChanged(Bool)
        case lookup(PresentationAction<LookupFeature.Action>)
        case delegate(Delegate)

        enum Delegate: Equatable {
            /// The page to reopen at while the book stays open.
            case pageShown(Int)
        }
    }

    @Dependency(\.pdfPagesClient) var pages
    @Dependency(\.uuid) var uuid

    var body: some ReducerOf<Self> {
        Reduce { state, action in
            switch action {
            case .task:
                guard state.texts.isEmpty else { return .none }
                let pdf = state.pdf
                return .run { send in
                    await send(.opened(Result { try await pages.pageTexts(pdf) }))
                }

            case let .opened(.success(texts)):
                guard !texts.isEmpty else {
                    state.errorMessage = "The PDF has no pages."
                    return .none
                }
                state.texts = texts
                if let fraction = state.startFraction { state.page = Int(fraction * Double(texts.count)) }
                return show(state.page, &state)

            case let .opened(.failure(error)):
                state.errorMessage = error.localizedDescription
                return .none

            case let .pageShown(page):
                guard page != state.page else { return .none }
                return show(page, &state)

            case .previousTapped:
                return show(state.page - 1, &state)

            case .nextTapped:
                return show(state.page + 1, &state)

            case let .characterTapped(index):
                guard state.texts.indices.contains(state.page) else { return .none }
                let text = PDFPageText(state.texts[state.page])
                // Between words, on a picture or in the margin, nothing is found.
                guard let offset = text.passageOffset(ofPageIndex: index),
                      let selection = WordContext(text: text.passage.text).selection(atUTF16Offset: offset)
                else { return .none }
                state.lookup = LookupFeature.State(
                    id: uuid(),
                    context: LookupContext(
                        word: selection.word,
                        sentence: selection.sentence,
                        language: state.language,
                        bookID: state.bookID
                    ),
                    scope: state.scope
                )
                state.marked = selection.wordRange.map(text.pageRange(ofPassage:))
                return .none

            case let .showsWordsChanged(shows):
                state.$showsWords.withLock { $0 = shows }
                markWords(&state)
                return .none

            case .lookup(.dismiss):
                state.marked = nil
                return .none

            case .lookup, .delegate:
                return .none
            }
        }
        .ifLet(\.$lookup, action: \.lookup) {
            LookupFeature()
        }
    }

    private func show(_ page: Int, _ state: inout State) -> Effect<Action> {
        let page = min(max(page, 0), state.pageCount - 1)
        guard page >= 0 else { return .none }
        if page != state.page { state.marked = nil }
        state.page = page
        markWords(&state)
        return .send(.delegate(.pageShown(page)))
    }

    private func markWords(_ state: inout State) {
        state.words = state.showsWords && state.texts.indices.contains(state.page)
            ? PDFPageText(state.texts[state.page]).wordRanges
            : []
    }
}
