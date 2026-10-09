import CoreGraphics
import Foundation

/// Finds the word a finger points at on a photographed page. A finger is
/// laid just short of the word it means, usually below it, so the word is
/// the one nearest ahead of the tip, along the way the finger points;
/// whatever is behind the tip is under the finger.
enum PointedWord {
    struct Place: Equatable, Sendable {
        var line: Int
        var word: Int
    }

    /// Nil when no finger was seen, or none points near the text — a hand
    /// holding the book rather than pointing into it.
    static func find(in photo: PhotoText) -> Place? {
        let heights = photo.lines.flatMap { $0.words.map(\.box.height) }.sorted()
        guard !heights.isEmpty else { return nil }
        let lineHeight = heights[heights.count / 2]

        var best: (place: Place, score: CGFloat)?
        for finger in photo.fingers {
            for (lineIndex, line) in photo.lines.enumerated() {
                for (wordIndex, word) in line.words.enumerated() {
                    guard let score = score(word.box, from: finger, lineHeight: lineHeight),
                          score < best?.score ?? .infinity
                    else { continue }
                    best = (Place(line: lineIndex, word: wordIndex), score)
                }
            }
        }
        return best?.place
    }

    /// How far the box is from the fingertip, counting a step aside from
    /// the finger's line against it as much as a step along it. Nil for a
    /// box behind the tip or out of its reach.
    private static func score(_ box: CGRect, from finger: PhotoText.Finger, lineHeight: CGFloat) -> CGFloat? {
        let tip = finger.tip
        let direction = finger.direction
        // The tip may rest on the lower part of the word it means.
        let ahead = (box.midX - tip.x) * direction.dx + (box.midY - tip.y) * direction.dy
        guard ahead > -lineHeight / 4 else { return nil }

        let dx = min(max(tip.x, box.minX), box.maxX) - tip.x
        let dy = min(max(tip.y, box.minY), box.maxY) - tip.y
        let distance = (dx * dx + dy * dy).squareRoot()
        guard distance < lineHeight * 3 else { return nil }
        let aside = abs(dx * direction.dy - dy * direction.dx)
        return distance + aside
    }
}
