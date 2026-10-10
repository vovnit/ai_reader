import Foundation

/// A picture as a grid of red, green and blue values, for looking at its
/// pixels one by one.
struct RGBImage: Sendable {
    let width: Int
    let height: Int
    /// Three values a pixel, 0 to 255, row after row from the top left.
    var values: [Float]

    func red(_ index: Int) -> Float { values[3 * index] }
    func green(_ index: Int) -> Float { values[3 * index + 1] }
    func blue(_ index: Int) -> Float { values[3 * index + 2] }

    /// The picture shrunk so its longer side is `longSide`, each new pixel
    /// the average of the area of the old ones it covers.
    func shrunk(toLongSide longSide: Int) -> RGBImage {
        let scale = min(1, Double(longSide) / Double(max(width, height)))
        let newWidth = max(1, Int((Double(width) * scale).rounded()))
        let newHeight = max(1, Int((Double(height) * scale).rounded()))
        let across = Self.shares(from: width, to: newWidth)
        let down = Self.shares(from: height, to: newHeight)

        var rows = [Float](repeating: 0, count: newWidth * height * 3)
        for y in 0..<height {
            for (x, share) in across.enumerated() {
                for (old, weight) in share {
                    for channel in 0..<3 {
                        rows[(y * newWidth + x) * 3 + channel] += weight * values[(y * width + old) * 3 + channel]
                    }
                }
            }
        }
        var shrunk = [Float](repeating: 0, count: newWidth * newHeight * 3)
        for (y, share) in down.enumerated() {
            for (old, weight) in share {
                for i in 0..<(newWidth * 3) {
                    shrunk[y * newWidth * 3 + i] += weight * rows[old * newWidth * 3 + i]
                }
            }
        }
        return RGBImage(width: newWidth, height: newHeight, values: shrunk)
    }

    /// For each new pixel along a side, the old pixels it covers and how
    /// much of it each one fills.
    private static func shares(from old: Int, to new: Int) -> [[(Int, Float)]] {
        let step = Double(old) / Double(new)
        return (0..<new).map { i in
            let start = Double(i) * step
            let end = start + step
            return (Int(start)..<min(old, Int(end.rounded(.up)))).compactMap { j in
                let cover = min(end, Double(j + 1)) - max(start, Double(j))
                return cover > 0 ? (j, Float(cover / step)) : nil
            }
        }
    }
}
