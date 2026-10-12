import ComposableArchitecture
import Foundation

/// A word on a printed page: the reader photographs the page with a finger
/// under the word, or picks a photo from the library, the photo's text is
/// read on the device, and the word the finger points at is explained in its
/// sentence — the same panel a book gives, and from there the next photo.
/// Where no finger points at a word, the text is shown to tap one in. There
/// is no book, so the lookup is filed under none.
@Reducer
struct PhotoFeature {
    enum Source: Equatable {
        case camera, library
    }

    @ObservableState
    struct State: Equatable {
        /// Where the next photo comes from while it is being picked; the
        /// camera is the first thing shown.
        var source: Source? = .camera
        var isReading = false
        var passage: PhotoPassage?
        var language: String?
        var errorMessage: String?
        /// The photo's text is shown, and the lookup is of a word tapped in it.
        var isChoosingWord = false
        @Presents var lookup: LookupFeature.State?
    }

    enum Action {
        case photoTaken(Data)
        case pickerCancelled
        case read(Result<PhotoText, any Error>)
        case nextPhotoTapped
        case libraryTapped
        case wordTapped(utf16Offset: Int)
        case doneTapped
        case lookup(PresentationAction<LookupFeature.Action>)
    }

    @Dependency(\.dismiss) var dismiss
    @Dependency(\.photoReaderClient) var reader
    @Dependency(\.uuid) var uuid

    var body: some ReducerOf<Self> {
        Reduce { state, action in
            switch action {
            case let .photoTaken(image):
                state.source = nil
                state.isReading = true
                state.passage = nil
                state.errorMessage = nil
                state.isChoosingWord = false
                state.lookup = nil
                return .run { send in
                    await send(.read(Result { try await reader.read(image: image) }))
                }

            case .pickerCancelled:
                // Before any photo, the way to the library or out; after
                // one, its word is still there.
                state.source = nil
                return .none

            case let .read(.success(photo)):
                state.isReading = false
                let passage = PhotoPassage(photo)
                guard !passage.text.isEmpty else {
                    state.errorMessage = "No text was found in the photo."
                    return .none
                }
                state.passage = passage
                state.language = TextLanguage.detect(in: passage.text)
                if let offset = passage.pointedOffset { explain(at: offset, &state) }
                state.isChoosingWord = state.lookup == nil
                return .none

            case let .read(.failure(error)):
                state.isReading = false
                state.errorMessage = error.localizedDescription
                return .none

            case .nextPhotoTapped:
                state.source = .camera
                return .none

            case .libraryTapped:
                state.source = .library
                return .none

            case let .wordTapped(offset):
                explain(at: offset, &state)
                return .none

            case .doneTapped:
                return .run { _ in await dismiss() }

            case .lookup(.dismiss):
                // A tapped word's explanation lies over the text, to tap the
                // next; a pointed word's is the whole screen, so its Done
                // closes it.
                guard !state.isChoosingWord else { return .none }
                return .run { _ in await dismiss() }

            case .lookup:
                return .none
            }
        }
        .ifLet(\.$lookup, action: \.lookup) {
            LookupFeature()
        }
    }

    private func explain(at offset: Int, _ state: inout State) {
        guard let text = state.passage?.text,
              let selection = WordContext(text: text).selection(atUTF16Offset: offset)
        else { return }
        state.lookup = LookupFeature.State(
            id: uuid(),
            context: LookupContext(word: selection.word, sentence: selection.sentence, language: state.language)
        )
    }
}
