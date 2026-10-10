import CoreGraphics
import Foundation

/// Finds a pointing finger the hand pose cannot see: a single finger that
/// comes into the photo from its edge, without the palm and knuckles Vision
/// needs to recognise a hand. It is a smooth, skin-coloured band that ends,
/// in a round cap, just below a word. Nil rather than a guess, since a wrong
/// finger would explain a word the reader never pointed at.
enum FingerFinder {
    /// `image` is the photo shrunk, its long side about 256 pixels — enough
    /// to keep a finger tens of pixels wide — and `lines` its text in the
    /// full photo's pixels.
    static func find(in image: RGBImage, lines: [PhotoText.Line], photoSize: CGSize) -> PhotoText.Finger? {
        let scale = CGFloat(image.width) / photoSize.width
        let boxes = lines.flatMap(\.words).map {
            CGRect(x: $0.box.minX * scale, y: $0.box.minY * scale, width: $0.box.width * scale, height: $0.box.height * scale)
        }
        let heights = lines.map(\.height).sorted()
        guard !heights.isEmpty, let mask = SkinMask(image: image, words: boxes) else { return nil }
        let lineHeight = heights[heights.count / 2]
        let up = lines.reduce(CGVector.zero) { CGVector(dx: $0.dx + $1.up.dx, dy: $0.dy + $1.up.dy) }
        let length = max((up.dx * up.dx + up.dy * up.dy).squareRoot(), 1e-6)
        let text = PhotoText(lines: lines, fingers: [])

        // A light finger first, then a dark one, taking in dimmer pixels but
        // asking for a closer likeness, since more of a dark background can
        // join it then.
        for (floor, fit) in [(Float(0.3), 0.65), (0.15, 0.8)] {
            var best: (finger: PhotoText.Finger, aim: CGFloat)?
            // A finger is wider than two lines of text and longer than it is wide.
            let area = Int(((2 * lineHeight * scale) * (2 * lineHeight * scale)).rounded(.up))
            for region in regions(of: mask, brighterThan: floor, minimumArea: area) {
                guard let shape = FingerShape(
                    pixels: region, width: mask.width, height: mask.height, fit: fit,
                    lineHeight: Double(lineHeight * scale), up: CGVector(dx: up.dx / length, dy: up.dy / length)
                ) else { continue }
                let finger = PhotoText.Finger(
                    tip: CGPoint(x: (shape.tip.x + 0.5) / scale, y: (shape.tip.y + 0.5) / scale),
                    direction: shape.direction
                )
                // A finger pointing at a word rests within a line and a half of it.
                guard let aim = PointedWord.aim(of: finger, in: text)?.score, aim <= 1.5 * lineHeight,
                      aim < best?.aim ?? .infinity
                else { continue }
                best = (finger, aim)
            }
            if let best { return best.finger }
        }
        return nil
    }

    /// The smooth skin-coloured regions, each cut from its neighbours where
    /// a weak edge joins them, then grown back over the outline, the nail
    /// and the folds the cut and the smoothness took.
    private static func regions(of mask: SkinMask, brighterThan floor: Float, minimumArea: Int) -> [[(x: Int, y: Int)]] {
        let skin = zip(mask.skin, mask.brightness).map { $0 && $1 > floor }
        var core = zip(mask.smooth, skin).map { $0 && $1 }
        for _ in 0..<2 { core = step(core, mask.width, mask.height, grow: false) }

        var seen = [Bool](repeating: false, count: core.count)
        var found: [[(x: Int, y: Int)]] = []
        for start in core.indices where core[start] && !seen[start] {
            var region = [Bool](repeating: false, count: core.count)
            var stack = [start], area = 0
            seen[start] = true
            while let i = stack.popLast() {
                region[i] = true
                area += 1
                let x = i % mask.width, y = i / mask.width
                for (nx, ny) in [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
                where nx >= 0 && nx < mask.width && ny >= 0 && ny < mask.height {
                    let j = ny * mask.width + nx
                    if core[j], !seen[j] { seen[j] = true; stack.append(j) }
                }
            }
            guard area >= minimumArea else { continue }
            for _ in 0..<3 {
                region = zip(step(region, mask.width, mask.height, grow: true), zip(skin, region)).map { $0 && $1.0 || $1.1 }
            }
            found.append(region.indices.filter { region[$0] }.map { (x: $0 % mask.width, y: $0 / mask.width) })
        }
        return found
    }

    /// The mask grown or shrunk by a pixel toward its four neighbours.
    private static func step(_ mask: [Bool], _ width: Int, _ height: Int, grow: Bool) -> [Bool] {
        var out = mask
        for y in 0..<height {
            for x in 0..<width {
                let i = y * width + x
                // The photo's edge neither grows nor wears the mask away.
                let left = x > 0 ? mask[i - 1] : !grow, right = x < width - 1 ? mask[i + 1] : !grow
                let above = y > 0 ? mask[i - width] : !grow, below = y < height - 1 ? mask[i + width] : !grow
                out[i] = grow ? mask[i] || left || right || above || below : mask[i] && left && right && above && below
            }
        }
        return out
    }
}
