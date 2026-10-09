import ComposableArchitecture
import CoreGraphics
import Foundation
import ImageIO
import NaturalLanguage
import Vision

/// Reads a photo of a page on the device, with Vision: its text, line by
/// line with where each word sits, and any index finger pointing into it.
@DependencyClient
struct PhotoReaderClient: Sendable {
    /// `image` is the photo as a JPEG or HEIC file holds it.
    var read: @Sendable (_ image: Data) async throws -> PhotoText
}

extension PhotoReaderClient: DependencyKey {
    static let liveValue = Self(read: { try PhotoReading.read($0) })

    static let testValue = Self()
}

extension DependencyValues {
    var photoReaderClient: PhotoReaderClient {
        get { self[PhotoReaderClient.self] }
        set { self[PhotoReaderClient.self] = newValue }
    }
}

private enum PhotoReading {
    struct UnreadablePhoto: LocalizedError {
        var errorDescription: String? { "The photo couldn’t be opened." }
    }

    static func read(_ data: Data) throws -> PhotoText {
        guard let source = CGImageSourceCreateWithData(data as CFData, nil),
              let image = CGImageSourceCreateImageAtIndex(source, 0, nil)
        else { throw UnreadablePhoto() }
        let properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [String: Any]
        let orientation = (properties?[kCGImagePropertyOrientation as String] as? UInt32)
            .flatMap(CGImagePropertyOrientation.init(rawValue:)) ?? .up
        let size = uprightSize(image, orientation)

        let handler = VNImageRequestHandler(cgImage: image, orientation: orientation)
        let text = VNRecognizeTextRequest()
        text.recognitionLevel = .accurate
        text.usesLanguageCorrection = true
        text.automaticallyDetectsLanguage = true
        try handler.perform([text])
        // Without the finger the reader can still tap a word, so a hand that
        // cannot be looked for is no reason to lose the text.
        let hands = VNDetectHumanHandPoseRequest()
        hands.maximumHandCount = 2
        try? handler.perform([hands])

        return PhotoText(
            lines: (text.results ?? []).compactMap { line($0, size) },
            fingers: (hands.results ?? []).compactMap { finger($0, size) }
        )
    }

    /// Vision measures the photo turned upright, and a camera often stores
    /// it on its side.
    private static func uprightSize(_ image: CGImage, _ orientation: CGImagePropertyOrientation) -> CGSize {
        switch orientation {
        case .left, .leftMirrored, .right, .rightMirrored:
            CGSize(width: image.height, height: image.width)
        default:
            CGSize(width: image.width, height: image.height)
        }
    }

    private static func line(_ observation: VNRecognizedTextObservation, _ size: CGSize) -> PhotoText.Line? {
        guard let candidate = observation.topCandidates(1).first else { return nil }
        let text = candidate.string
        var words: [PhotoText.Word] = []
        let tokenizer = NLTokenizer(unit: .word)
        tokenizer.string = text
        tokenizer.enumerateTokens(in: text.startIndex..<text.endIndex) { range, _ in
            if let box = try? candidate.boundingBox(for: range)?.boundingBox {
                words.append(PhotoText.Word(range: NSRange(range, in: text), box: pixels(box, size)))
            }
            return true
        }
        return PhotoText.Line(text: text, words: words)
    }

    /// The index finger, when its tip is seen. It points from the nearest
    /// joint seen below the tip; with none seen, up the page, the way a
    /// finger under a line usually points.
    private static func finger(_ hand: VNHumanHandPoseObservation, _ size: CGSize) -> PhotoText.Finger? {
        guard let tip = try? hand.recognizedPoint(.indexTip), tip.confidence > 0.3 else { return nil }
        let end = pixels(tip.location, size)
        var direction = CGVector(dx: 0, dy: -1)
        for name in [VNHumanHandPoseObservation.JointName.indexDIP, .indexPIP, .indexMCP] {
            guard let joint = try? hand.recognizedPoint(name), joint.confidence > 0.3 else { continue }
            let start = pixels(joint.location, size)
            let length = hypot(end.x - start.x, end.y - start.y)
            if length > 0 {
                direction = CGVector(dx: (end.x - start.x) / length, dy: (end.y - start.y) / length)
            }
            break
        }
        return PhotoText.Finger(tip: end, direction: direction)
    }

    /// Vision's positions are fractions of the photo from its bottom left corner.
    private static func pixels(_ box: CGRect, _ size: CGSize) -> CGRect {
        CGRect(
            x: box.minX * size.width,
            y: (1 - box.maxY) * size.height,
            width: box.width * size.width,
            height: box.height * size.height
        )
    }

    private static func pixels(_ point: CGPoint, _ size: CGSize) -> CGPoint {
        CGPoint(x: point.x * size.width, y: (1 - point.y) * size.height)
    }
}
