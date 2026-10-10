import CoreGraphics
import Foundation

/// The pixels of a shrunk photo that could be a finger. Colours are judged
/// against the page's own white, so skin stays skin under warm, cool or
/// coloured light, and texture against the print's own, so the size and
/// sharpness of the photo do not matter.
struct SkinMask {
    let width: Int
    let height: Int
    /// Red the strongest, with a skin's hue and saturation once balanced to
    /// the page's white: never paper, its shadows, ink or tiles.
    let skin: [Bool]
    /// Far less textured than the print: never words or a picture's detail.
    let smooth: [Bool]
    /// The brightest channel, against the page's white.
    let brightness: [Float]

    /// `words` are the word boxes, in the shrunk photo's pixels.
    init?(image: RGBImage, words: [CGRect]) {
        width = image.width
        height = image.height
        let inWords = Self.cover(words, width: width, height: height)
        let wordPixels = inWords.indices.filter { inWords[$0] }
        guard !wordPixels.isEmpty else { return nil }

        // The page's white is the brightest tenth of what lies inside the
        // word boxes, which is mostly the paper between strokes.
        let sums = wordPixels.map { image.red($0) + image.green($0) + image.blue($0) }
        let brightest = Self.percentile(sums.sorted(), 0.9)
        let white = zip(wordPixels, sums).filter { $0.1 >= brightest }.map(\.0)
        let whiteRed = max(1, white.map(image.red).reduce(0, +) / Float(white.count))
        let whiteGreen = max(1, white.map(image.green).reduce(0, +) / Float(white.count))
        let whiteBlue = max(1, white.map(image.blue).reduce(0, +) / Float(white.count))

        var balanced = [Float](repeating: 0, count: width * height * 3)
        var skin = [Bool](repeating: false, count: width * height)
        var brightness = [Float](repeating: 0, count: width * height)
        for i in 0..<(width * height) {
            let red = image.red(i) / whiteRed, green = image.green(i) / whiteGreen, blue = image.blue(i) / whiteBlue
            balanced[3 * i] = red
            balanced[3 * i + 1] = green
            balanced[3 * i + 2] = blue
            let low = min(green, blue)
            let hue = 60 * (green - blue) / max(red - low, 1e-6)
            skin[i] = red >= max(green, blue) && hue > -25 && hue < 40 && red - low > 0.15 * red
            brightness[i] = max(red, green, blue)
        }
        self.skin = skin
        self.brightness = brightness

        let texture = Self.texture(balanced, width: width, height: height)
        let printTexture = Self.median(wordPixels.map { texture[$0] })
        smooth = texture.map { $0 < 0.4 * printTexture }
    }

    /// The colour step across each pixel, to the left and right and up and
    /// down, averaged over its 3 by 3 neighbourhood.
    private static func texture(_ rgb: [Float], width: Int, height: Int) -> [Float] {
        var step = [Float](repeating: 0, count: width * height)
        for y in 0..<height {
            for x in 0..<width {
                let i = y * width + x
                for channel in 0..<3 {
                    if x > 0, x < width - 1 { step[i] += abs(rgb[3 * (i + 1) + channel] - rgb[3 * (i - 1) + channel]) }
                    if y > 0, y < height - 1 { step[i] += abs(rgb[3 * (i + width) + channel] - rgb[3 * (i - width) + channel]) }
                }
            }
        }
        var averaged = [Float](repeating: 0, count: width * height)
        for y in 0..<height {
            for x in 0..<width {
                var sum: Float = 0
                for dy in -1...1 {
                    for dx in -1...1 {
                        sum += step[min(max(y + dy, 0), height - 1) * width + min(max(x + dx, 0), width - 1)]
                    }
                }
                averaged[y * width + x] = sum / 9
            }
        }
        return averaged
    }

    private static func cover(_ boxes: [CGRect], width: Int, height: Int) -> [Bool] {
        var covered = [Bool](repeating: false, count: width * height)
        for box in boxes {
            let x0 = max(0, Int(box.minX)), x1 = min(width, Int(box.maxX.rounded(.up)))
            let y0 = max(0, Int(box.minY)), y1 = min(height, Int(box.maxY.rounded(.up)))
            guard x0 < x1, y0 < y1 else { continue }
            for y in y0..<y1 {
                for x in x0..<x1 { covered[y * width + x] = true }
            }
        }
        return covered
    }

    /// Between the two nearest values, as numpy's percentile does.
    static func percentile(_ sorted: [Float], _ fraction: Float) -> Float {
        let position = fraction * Float(sorted.count - 1)
        let below = Int(position)
        let above = min(below + 1, sorted.count - 1)
        return sorted[below] + (sorted[above] - sorted[below]) * (position - Float(below))
    }

    static func median(_ values: [Float]) -> Float {
        let sorted = values.sorted()
        let middle = sorted.count / 2
        return sorted.count % 2 == 1 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2
    }
}
