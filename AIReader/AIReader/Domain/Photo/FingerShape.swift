import CoreGraphics
import Foundation

/// Whether a region of a shrunk photo is shaped like a pointing finger, and
/// where its tip is: a band at least two lines wide that comes in from the
/// photo's edge, up the page or across it, and ends in a round cap inside it.
struct FingerShape {
    let tip: CGPoint
    /// The way to the word: leaning twice as much to the page's up as to
    /// the finger's own axis, since readers hold the finger under the word.
    let direction: CGVector

    /// `pixels` are the region's, as (x, y) in a photo `width` by `height`;
    /// `fit` is how closely the end must match an ideal finger, from 0 to 1.
    init?(pixels: [(x: Int, y: Int)], width: Int, height: Int, fit: Double, lineHeight: Double, up: CGVector) {
        let points = pixels.map { (x: Double($0.x), y: Double($0.y)) }
        let entry = points.filter { $0.x < 2 || $0.y < 2 || $0.x >= Double(width - 2) || $0.y >= Double(height - 2) }
        guard !entry.isEmpty else { return nil }

        // From where it enters toward its middle, then along the far end
        // alone, not the hand behind it.
        let centre = Self.mean(points), door = Self.mean(entry)
        var axis = Self.unit(centre.x - door.x, centre.y - door.y)
        let across = (12 * max(0, Self.spread(points).least)).squareRoot()
        for _ in 0..<2 {
            let front = Self.front(points, axis)
            let end = points.filter { Self.depth($0, axis, front) < 2 * across }
            let major = Self.spread(end).major
            axis = major.x * axis.x + major.y * axis.y > 0 ? major : (-major.x, -major.y)
        }
        // Hanging down the page: a picture or the room, not a pointing finger.
        guard axis.x * up.dx + axis.y * up.dy >= -0.25 else { return nil }

        let front = Self.front(points, axis)
        let depths = points.map { Self.depth($0, axis, front) }
        let slices = Self.slices(depths)
        guard let wide = Self.width(slices, start: across), wide >= 2 * lineHeight,
              Self.fit(points, depths, axis, front, wide, width: width, height: height) >= fit,
              // An eighth of a width behind the tip a half disc is two
              // thirds as wide as the finger; a box or a band is as wide.
              Double(slices[Int(wide / 8)]) <= 0.8 * wide
        else { return nil }

        let lead = Self.unit(axis.x + 2 * up.dx, axis.y + 2 * up.dy)
        let cap = zip(points, depths).filter { $0.1 < wide / 2 }.map(\.0)
        let reach = cap.map { $0.x * lead.x + $0.y * lead.y }
        let farthest = reach.max() ?? 0
        let foremost = Self.mean(zip(cap, reach).filter { $0.1 >= farthest - 1 }.map(\.0))
        // Where the region leaves the photo no tip is in sight.
        guard foremost.x >= 1, foremost.x < Double(width - 2), foremost.y >= 1, foremost.y < Double(height - 2)
        else { return nil }
        tip = CGPoint(x: foremost.x + lead.x, y: foremost.y + lead.y)
        direction = CGVector(dx: lead.x, dy: lead.y)
    }

    /// The finger's width: the typical slice of its far end, measured again
    /// from that first guess.
    private static func width(_ slices: [Int], start: Double) -> Double? {
        var wide = start
        for _ in 0..<2 {
            let range = min(slices.count, Int(wide / 2))..<min(slices.count, Int(2 * wide))
            let counts = slices[range].filter { $0 > 0 }.map(Float.init)
            guard !counts.isEmpty else { return nil }
            wide = Double(SkinMask.median(counts))
        }
        return wide
    }

    /// How much the far end overlaps an ideal finger, a band its width that
    /// ends in a half disc, over its first two widths within the photo.
    private static func fit(_ points: [(x: Double, y: Double)], _ depths: [Double], _ axis: (x: Double, y: Double),
                            _ front: Double, _ wide: Double, width: Int, height: Int) -> Double {
        let radius = wide / 2
        let length = min(2 * wide, (depths.max() ?? 0) + 1)
        // Too little shows to tell a finger from any round blob.
        guard length >= wide else { return 0 }
        let sides = zip(points, depths)
            .filter { $0.1 < length && $0.1 >= radius }
            .map { Float(-$0.0.x * axis.y + $0.0.y * axis.x) }
        guard !sides.isEmpty else { return 0 }
        let middle = Double(SkinMask.median(sides))
        var region = Set<Int>()
        for (point, depth) in zip(points, depths) where depth < length {
            region.insert(Int(point.y) * width + Int(point.x))
        }
        var ideal = 0, both = 0
        for y in 0..<height {
            for x in 0..<width {
                let depth = front - (Double(x) * axis.x + Double(y) * axis.y)
                guard depth >= 0, depth < length else { continue }
                let half = depth < radius ? max(0, radius * radius - (radius - depth) * (radius - depth)).squareRoot() : radius
                guard abs(-Double(x) * axis.y + Double(y) * axis.x - middle) <= half + 0.5 else { continue }
                ideal += 1
                if region.contains(y * width + x) { both += 1 }
            }
        }
        return Double(both) / Double(ideal + region.count - both)
    }

    private static func front(_ points: [(x: Double, y: Double)], _ axis: (x: Double, y: Double)) -> Double {
        points.map { $0.x * axis.x + $0.y * axis.y }.max() ?? 0
    }

    private static func depth(_ point: (x: Double, y: Double), _ axis: (x: Double, y: Double), _ front: Double) -> Double {
        front - (point.x * axis.x + point.y * axis.y)
    }

    /// How many pixels lie at each whole depth behind the front.
    private static func slices(_ depths: [Double]) -> [Int] {
        var counts = [Int](repeating: 0, count: Int(depths.max() ?? 0) + 1)
        for depth in depths { counts[Int(depth)] += 1 }
        return counts
    }

    private static func mean(_ points: [(x: Double, y: Double)]) -> (x: Double, y: Double) {
        let n = Double(max(points.count, 1))
        return (points.map(\.x).reduce(0, +) / n, points.map(\.y).reduce(0, +) / n)
    }

    private static func unit(_ x: Double, _ y: Double) -> (x: Double, y: Double) {
        let length = max((x * x + y * y).squareRoot(), 1e-6)
        return (x / length, y / length)
    }

    /// The points' covariance: its smaller eigenvalue, and the direction of
    /// its larger one.
    private static func spread(_ points: [(x: Double, y: Double)]) -> (least: Double, major: (x: Double, y: Double)) {
        let centre = mean(points)
        let n = Double(max(points.count - 1, 1))
        let a = points.map { ($0.x - centre.x) * ($0.x - centre.x) }.reduce(0, +) / n
        let b = points.map { ($0.x - centre.x) * ($0.y - centre.y) }.reduce(0, +) / n
        let c = points.map { ($0.y - centre.y) * ($0.y - centre.y) }.reduce(0, +) / n
        let root = (((a - c) / 2) * ((a - c) / 2) + b * b).squareRoot()
        let largest = (a + c) / 2 + root
        let major = b != 0 ? unit(largest - c, b) : (a >= c ? (1.0, 0.0) : (0.0, 1.0))
        return ((a + c) / 2 - root, major)
    }
}
