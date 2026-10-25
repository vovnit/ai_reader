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
        let text = PhotoText(lines: lines, fingers: [], size: photoSize)

        // A light finger first, then a dark one, taking in dimmer pixels but
        // asking for a closer likeness, since more of a dark background can
        // join it then.
        for (floor, fit) in [(Float(0.3), 0.65), (0.15, 0.8)] {
            var best: (finger: PhotoText.Finger, aim: CGFloat)?
            // A finger is wider than two lines of text and longer than it is wide.
            let area = Int(((2 * lineHeight * scale) * (2 * lineHeight * scale)).rounded(.up))
            for region in candidates(in: mask, brighterThan: floor, minimumArea: area) {
                guard let shape = FingerShape(
                    pixels: region, width: mask.width, height: mask.height, fit: fit,
                    lineHeight: Double(lineHeight * scale), up: CGVector(dx: up.dx / length, dy: up.dy / length)
                ), !surroundsPrint(region, boxes: boxes, mask.width, mask.height, fingerArea: area) else { continue }
                let finger = PhotoText.Finger(
                    tip: CGPoint(x: (shape.tip.x + 0.5) / scale, y: (shape.tip.y + 0.5) / scale),
                    direction: shape.direction
                )
                // A finger pointing at a word rests within a line and a half of it.
                guard let pointed = PointedWord.aim(of: finger, in: text), pointed.score <= 1.5 * lineHeight,
                      pointed.score < best?.aim ?? .infinity
                else { continue }
                // Print on a skin-coloured page joins the page to the finger,
                // and the page's own edge can pass for one: a finger is told
                // from the paper by the word it points at, printed on
                // something else.
                let box = lines[pointed.place.line].words[pointed.place.word].box
                guard !onSkin(mask, CGRect(x: box.minX * scale, y: box.minY * scale, width: box.width * scale, height: box.height * scale))
                else { continue }
                best = (finger, pointed.score)
            }
            if let best { return best.finger }
        }
        return nil
    }

    /// Whether the region closes around a patch of print wider than a
    /// finger, as a skin-coloured page does around a white box and a finger
    /// never does; paper in a hand's shadow, skin-coloured too, closes only
    /// around a word's ink. `boxes` are the words', in the shrunk photo's
    /// pixels, and `fingerArea` the least a finger covers.
    private static func surroundsPrint(
        _ region: [(x: Int, y: Int)], boxes: [CGRect], _ width: Int, _ height: Int, fingerArea: Int
    ) -> Bool {
        var inRegion = [Bool](repeating: false, count: width * height)
        for point in region { inRegion[point.y * width + point.x] = true }
        let outsideRegion = inRegion.map { !$0 }
        // What can be reached from the photo's edge without crossing the region.
        let edge = (0..<(width * height)).map { i in
            let x = i % width, y = i / width
            return x == 0 || y == 0 || x == width - 1 || y == height - 1
        }
        let outside = flood(outsideRegion, from: edge, width, height)
        let hole = zip(outsideRegion, outside).map { $0 && !$1 }
        return boxes.contains { box in
            let x = min(width - 1, max(0, Int(box.midX))), y = min(height - 1, max(0, Int(box.midY)))
            guard hole[y * width + x] else { return false }
            var seed = [Bool](repeating: false, count: width * height)
            seed[y * width + x] = true
            return flood(hole, from: seed, width, height).filter { $0 }.count > 4 * fingerArea
        }
    }

    /// Whether most of `box`, in the shrunk photo's pixels, is skin-coloured.
    private static func onSkin(_ mask: SkinMask, _ box: CGRect) -> Bool {
        let covered = SkinMask.cover([box], width: mask.width, height: mask.height)
        let inside = covered.filter { $0 }.count
        let skin = zip(covered, mask.skin).filter { $0 && $1 }.count
        return 2 * skin > inside
    }

    /// The smooth skin-coloured regions, each cut from its neighbours where
    /// a weak edge joins them, then grown back over the outline, the nail
    /// and the folds the cut and the smoothness took; and each one's whole
    /// skin-coloured area too, since on print with little texture to judge
    /// by, smoothness keeps only part of a finger lit from the side.
    private static func candidates(in mask: SkinMask, brighterThan floor: Float, minimumArea: Int) -> [[(x: Int, y: Int)]] {
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
            var grown = region
            for _ in 0..<3 {
                grown = zip(step(grown, mask.width, mask.height, grow: true), zip(skin, grown)).map { $0 && $1.0 || $1.1 }
            }
            let whole = flood(skin, from: region, mask.width, mask.height)
            for candidate in whole == grown ? [grown] : [grown, whole] {
                found.append(candidate.indices.filter { candidate[$0] }.map { (x: $0 % mask.width, y: $0 / mask.width) })
            }
        }
        return found
    }

    /// The part of `mask` joined to `seed` through its four neighbours.
    static func flood(_ mask: [Bool], from seed: [Bool], _ width: Int, _ height: Int) -> [Bool] {
        var out = zip(seed, mask).map { $0 && $1 }
        var stack = out.indices.filter { out[$0] }
        while let i = stack.popLast() {
            let x = i % width, y = i / width
            for (nx, ny) in [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
            where nx >= 0 && nx < width && ny >= 0 && ny < height {
                let j = ny * width + nx
                if mask[j], !out[j] { out[j] = true; stack.append(j) }
            }
        }
        return out
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
