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
        var text = ""
        var lineStarts: [Int] = []
        for line in photo.lines {
            // Joined the way a PDF's lines are, a word hyphenated at a line's end mended.
            text = text.isEmpty ? line.text : PDFParagraphs.joined(text, line.text)
            lineStarts.append(text.utf16.count - line.text.utf16.count)
        }
        self.text = text
        pointedOffset = PointedWord.find(in: photo).map { place in
            lineStarts[place.line] + photo.lines[place.line].words[place.word].range.location
        }
        words = zip(photo.lines, lineStarts).flatMap { PhotoWord.words(of: $0, at: $1) }
        photoSize = photo.size
    }
}
