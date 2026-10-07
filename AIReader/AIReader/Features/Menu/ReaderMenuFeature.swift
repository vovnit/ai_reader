import ComposableArchitecture
import Foundation

/// The reader's menu: the book's table of contents, what has been looked up
/// in it, a search of it, an X-ray of a name, a conversation about the page on
/// screen, how the text is rendered, and the book's offline glossary.
@Reducer
struct ReaderMenuFeature {
    @Reducer
    enum Path {
        case chat(ChatFeature)
        case contents(ContentsFeature)
        case display(DisplaySettingsFeature)
        case glossary(GlossaryFeature)
        case search(SearchFeature)
        case words(WordsFeature)
        case xray(XRayFeature)
    }

    @ObservableState
    struct State: Equatable {
        let bookID: Book.ID
        let title: String
        /// The text of the page the reader is on, handed to the chat.
        let page: String
        /// The books to search and how far they have been read.
        let scope: ReadingScope
        /// "this book" or the group, for the search screen.
        let covers: String
        /// The book's table of contents, and the entry the page falls under.
        let contents: [ContentsEntry]
        let currentEntry: Int?
        var path = StackState<Path.State>()
    }

    enum Action {
        case chatTapped
        case closeBookTapped
        case contentsTapped
        case displayTapped
        case doneTapped
        case glossaryTapped
        case searchTapped
        case wordsTapped
        case xrayTapped
        case path(StackActionOf<Path>)
        case delegate(Delegate)

        enum Delegate: Equatable {
            case closeBook
            case dismiss
            case jump(BookPosition)
        }
    }

    var body: some ReducerOf<Self> {
        Reduce { state, action in
            switch action {
            case .chatTapped:
                state.path.append(.chat(.aboutPage(state.page, scope: state.scope)))
                return .none

            case .contentsTapped:
                state.path.append(.contents(ContentsFeature.State(
                    bookID: state.bookID,
                    entries: state.contents,
                    current: state.currentEntry
                )))
                return .none

            case .displayTapped:
                state.path.append(.display(DisplaySettingsFeature.State()))
                return .none

            case .glossaryTapped:
                state.path.append(.glossary(GlossaryFeature.State(
                    bookID: state.bookID,
                    title: state.title,
                    scope: state.scope
                )))
                return .none

            case .searchTapped:
                state.path.append(.search(SearchFeature.State(scope: state.scope, covers: state.covers)))
                return .none

            case .wordsTapped:
                state.path.append(.words(WordsFeature.State(bookID: state.bookID)))
                return .none

            case .xrayTapped:
                state.path.append(.xray(XRayFeature.State(term: "", scope: state.scope)))
                return .none

            case .closeBookTapped:
                return .send(.delegate(.closeBook))

            case .doneTapped:
                return .send(.delegate(.dismiss))

            case let .path(.element(id: _, action: .contents(.delegate(.jump(position))))),
                 let .path(.element(id: _, action: .search(.delegate(.jump(position))))),
                 let .path(.element(id: _, action: .xray(.delegate(.jump(position))))):
                return .send(.delegate(.jump(position)))

            case let .path(.element(id: _, action: .xray(.delegate(.ask(chat))))):
                state.path.append(.chat(chat))
                return .none

            case .path, .delegate:
                return .none
            }
        }
        .forEach(\.path, action: \.path)
    }
}

extension ReaderMenuFeature.Path.State: Equatable {}
