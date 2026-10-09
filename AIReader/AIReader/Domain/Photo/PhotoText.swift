import CoreGraphics
import Foundation

/// What was read off a photo of a page: its lines, with where each word
/// sits, and the fingers seen pointing into it. Positions are in the
/// photo's pixels, from its top left corner, the photo turned upright.
struct PhotoText: Equatable, Sendable {
    struct Word: Equatable, Sendable {
        /// Where the word is in its line's text, in UTF-16 units.
        var range: NSRange
        var box: CGRect
    }

    struct Line: Equatable, Sendable {
        var text: String
        var words: [Word]
    }

    /// The tip of an index finger, and the way it points: from the joint
    /// below the tip toward it, one pixel long.
    struct Finger: Equatable, Sendable {
        var tip: CGPoint
        var direction: CGVector
    }

    var lines: [Line]
    var fingers: [Finger]
}
