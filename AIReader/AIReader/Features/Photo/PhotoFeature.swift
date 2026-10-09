import ComposableArchitecture
import Foundation

/// A word on a printed page: the reader photographs the page with a finger
/// under the word, the photo's text is read on the device, and the word the
/// finger points at is explained in its sentence — the same panel a book
/// gives. The text stays to tap another word in, when the finger was missed
/// or meant another. There is no book, so the lookup is filed under none.
@Reducer
struct PhotoFeature {
    @ObservableState
    struct State: Equatable {
        /// The camera is the first thing shown.
        var isTakingPhoto = true
        var isReading = false
        var passage: PhotoPassage?
        var chunks: [WordContext.Chunk] = []
        var language: String?
        var errorMessage: String?
        @Presents var lookup: LookupFeature.State?
    }

    enum Action {
        case photoTaken(Data)
        case cameraCancelled
        case read(Result<PhotoText, any Error>)
        case retakeTapped
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
                state.isTakingPhoto = false
                state.isReading = true
                state.errorMessage = nil
                return .run { send in
                    await send(.read(Result { try await reader.read(image: image) }))
                }

            case .cameraCancelled:
                // Before any photo there is nothing to go back to.
                guard state.passage != nil || state.errorMessage != nil else {
                    return .run { _ in await dismiss() }
                }
                state.isTakingPhoto = false
                return .none

            case let .read(.success(photo)):
                state.isReading = false
                let passage = PhotoPassage(photo)
                guard !passage.text.isEmpty else {
                    state.passage = nil
                    state.chunks = []
                    state.errorMessage = "No text was found in the photo."
                    return .none
                }
                state.passage = passage
                state.chunks = WordContext(text: passage.text).chunks
                state.language = TextLanguage.detect(in: passage.text)
                if let offset = passage.pointedOffset { explain(at: offset, &state) }
                return .none

            case let .read(.failure(error)):
                state.isReading = false
                state.errorMessage = error.localizedDescription
                return .none

            case .retakeTapped:
                state.isTakingPhoto = true
                return .none

            case let .wordTapped(offset):
                explain(at: offset, &state)
                return .none

            case .doneTapped:
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
