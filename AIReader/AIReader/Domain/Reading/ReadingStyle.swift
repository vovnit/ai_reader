import ComposableArchitecture
import Foundation

#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// How the book is rendered: the reader's own preference, applied on top of
/// whatever fonts the EPUB itself carries.
struct ReadingStyle: Equatable, Sendable, Codable {
    /// Multiplier on the size the book asks for.
    var scale: Double = 1.4
    /// A font family to impose, or nil to keep the book's own faces.
    var fontName: String?
    var lineSpacing: Double = 2
    var margin: Double = 24

    static let scaleRange = 0.8...2.6
    static let lineSpacingRange = 0.0...16.0
    static let marginRange = 8.0...64.0

    /// Families worth offering: the book's own, plus faces that read well at
    /// length.
    static let fontNames = [
        "Georgia", "Palatino", "Times New Roman", "Charter",
        "Iowan Old Style", "Helvetica Neue", "Verdana"
    ]

    /// Where text is laid out inside a container: the container less the
    /// margins, and no wider than a line the eye can follow back — on an iPad
    /// or a Mac window a full-width line runs well past a hundred characters.
    func pageSize(in container: CGSize) -> CGSize {
        // About 65 characters at the body size the scale applies to.
        let measure = 32 * 17 * scale
        return CGSize(
            width: max(min(container.width - margin * 2, measure), 1),
            height: max(container.height - margin * 2, 1)
        )
    }
}

extension SharedReaderKey where Self == FileStorageKey<ReadingStyle>.Default {
    /// One shared value: the menu edits it, the reader renders from it, and it
    /// is written to disk on every change.
    static var readingStyle: Self {
        Self[
            .fileStorage(
                URL.applicationSupportDirectory.appending(path: "reading-style.json")
            ),
            default: ReadingStyle()
        ]
    }
}
