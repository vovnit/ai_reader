import CoreGraphics
import Foundation

/// A word read off a photo, placed to lay its text over the print it was
/// read from, and where it starts in the photo's passage. Places are in
/// the photo's pixels.
struct PhotoWord: Equatable, Sendable {
    /// As its line has it, a hyphen at the line's end kept.
    var text: String
    /// A UTF-16 offset into the passage's text.
    var offset: Int
    var center: CGPoint
    /// Along the line, and across it: the line's height.
    var size: CGSize
    /// The way the line reads, in radians clockwise from the photo's x axis.
    var angle: Double

    /// The words of `line`, which starts at `lineStart` in the passage.
    static func words(of line: PhotoText.Line, at lineStart: Int) -> [PhotoWord] {
        let up = line.up
        let along = CGVector(dx: -up.dy, dy: up.dx)
        let cos = abs(along.dx), sin = abs(along.dy)
        // Each word is centred across where most of the line's are, as a
        // box reaching up to an ascender or down to a descender is not.
        let across = line.words.map { $0.box.midX * up.dx + $0.box.midY * up.dy }.sorted()
        let middle = across.isEmpty ? 0 : across[across.count / 2]
        return line.words.map { word in
            let box = word.box
            // The box stands upright around the word turned with the page,
            // its sides the word's length and the line's height crossed.
            let length = cos >= sin
                ? (box.width - line.height * sin) / cos
                : (box.height - line.height * cos) / sin
            let shift = middle - (box.midX * up.dx + box.midY * up.dy)
            return PhotoWord(
                text: (line.text as NSString).substring(with: word.range),
                offset: lineStart + word.range.location,
                center: CGPoint(x: box.midX + shift * up.dx, y: box.midY + shift * up.dy),
                // At least a letter wide, which a slanted box can come short of.
                size: CGSize(width: max(length, line.height / 2), height: line.height),
                angle: atan2(along.dy, along.dx)
            )
        }
    }
}
