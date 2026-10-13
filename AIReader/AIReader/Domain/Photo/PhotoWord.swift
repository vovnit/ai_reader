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
    /// Along the line, and across it: its print's height.
    var size: CGSize
    /// The way the line reads, in radians clockwise from the photo's x axis.
    var angle: Double

    /// The words of `line`, which starts at `lineStart` in the passage.
    static func words(of line: PhotoText.Line, at lineStart: Int) -> [PhotoWord] {
        let up = line.up
        let along = CGVector(dx: -up.dy, dy: up.dx)
        let cos = abs(along.dx), sin = abs(along.dy)
        let across = line.words.map { $0.box.midX * up.dx + $0.box.midY * up.dy }
        let print = printBand(of: line, across: across, cos: cos, sin: sin)
        return line.words.enumerated().map { index, word in
            let box = word.box
            let text = (line.text as NSString).substring(with: word.range)
            // A word read with a quote or a dash before it starts at its
            // first letter or digit, where a tap finds it.
            let lead = (text as NSString).rangeOfCharacter(from: .alphanumerics).location
            let height = boxHeight(box, cos: cos, sin: sin) ?? print.height
            // The box stands upright around the word turned with the page,
            // its sides the word's length and its height crossed.
            let length = cos >= sin
                ? (box.width - height * sin) / cos
                : (box.height - height * cos) / sin
            // Every word sits on the line's print, as a box reaching up to
            // an ascender or down to a descender does not.
            let shift = print.middle - across[index]
            return PhotoWord(
                text: text,
                offset: lineStart + word.range.location + (lead == NSNotFound ? 0 : lead),
                center: CGPoint(x: box.midX + shift * up.dx, y: box.midY + shift * up.dy),
                // At least a letter wide, which a slanted box can come short of.
                size: CGSize(width: max(length, print.height / 2), height: print.height),
                angle: atan2(along.dy, along.dx)
            )
        }
    }

    /// Where the line's print lies across it, along its `up`: from the top
    /// of its tallest letter to the foot of its lowest. Near half a right
    /// angle a box's sides cannot be told apart, and the line's own height
    /// is taken, about the middle of its words.
    private static func printBand(
        of line: PhotoText.Line, across: [CGFloat], cos: CGFloat, sin: CGFloat
    ) -> (middle: CGFloat, height: CGFloat) {
        let heights = line.words.compactMap { boxHeight($0.box, cos: cos, sin: sin) }
        guard !heights.isEmpty, heights.count == across.count else {
            let sorted = across.sorted()
            return (sorted.isEmpty ? 0 : sorted[sorted.count / 2], line.height)
        }
        let top = zip(across, heights).map { $0 + $1 / 2 }.max()!
        let foot = zip(across, heights).map { $0 - $1 / 2 }.min()!
        return ((top + foot) / 2, max(top - foot, 1))
    }

    /// How tall the word in `box` is. Once the page is turned, an upright
    /// box mixes the word's length into its height; its two sides give
    /// both back, except near half a right angle, where they are alike:
    /// then nil.
    private static func boxHeight(_ box: CGRect, cos: CGFloat, sin: CGFloat) -> CGFloat? {
        let turn = cos * cos - sin * sin
        return abs(turn) < 0.2 ? nil : max((box.height * cos - box.width * sin) / turn, 0)
    }
}
