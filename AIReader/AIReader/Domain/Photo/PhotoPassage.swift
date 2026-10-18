import CoreGraphics
import Foundation

/// A photo's lines run together as the page reads, so the sentence around
/// a word can be found across line breaks, and where in it the word the
/// finger points at starts — or, with no finger, each word read off the
/// photo, to tap one where it is printed.
struct PhotoPassage: Equatable, Sendable {
    let text: String
    /// A UTF-16 offset into `text`; nil when no finger pointed at a word.
    let pointedOffset: Int?
    let words: [PhotoWord]
    /// The photo's size, upright, which the words' places are in.
    let photoSize: CGSize

    init(_ photo: PhotoText) {
        let passage = PagePassage(lines: photo.lines.map(\.text))
        text = passage.text
        let lineWords = zip(photo.lines, passage.lineStarts).map { PhotoWord.words(of: $0, at: $1) }
        pointedOffset = PointedWord.find(in: photo).map { lineWords[$0.line][$0.word].offset }
        words = lineWords.flatMap { $0 }
        photoSize = photo.size
    }
}
