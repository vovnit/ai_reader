import Foundation

#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// Splits a book into page-sized character ranges with TextKit, so each page can
/// be laid out in a text view of the same size.
enum Paginator {
    static func pages(of text: NSAttributedString, size: CGSize) -> [NSRange] {
        guard text.length > 0, size.width > 1, size.height > 1 else { return [] }

        let storage = NSTextStorage(attributedString: text)
        let layout = NSLayoutManager()
        storage.addLayoutManager(layout)

        var ranges: [NSRange] = []
        var glyph = 0
        while glyph < layout.numberOfGlyphs {
            let container = NSTextContainer(size: size)
            container.lineFragmentPadding = 0
            layout.addTextContainer(container)

            let glyphs = layout.glyphRange(for: container)
            guard glyphs.length > 0 else { break }
            ranges.append(layout.characterRange(forGlyphRange: glyphs, actualGlyphRange: nil))
            glyph = NSMaxRange(glyphs)
        }
        return ranges
    }

    /// Shrinks illustrations that are wider or taller than a page. Attachments
    /// are reference types, so only their layout box changes here.
    ///
    /// A picture is fitted to slightly less than the full page so the line it
    /// sits on, with its spacing, still fits — otherwise it is pushed onto the
    /// next page and leaves an empty one behind.
    static func fitImages(in text: NSAttributedString, to size: CGSize) {
        let box = CGSize(width: size.width, height: size.height * 0.92)
        let whole = NSRange(location: 0, length: text.length)
        text.enumerateAttribute(.attachment, in: whole) { value, _, _ in
            guard let attachment = value as? NSTextAttachment,
                  let image = attachment.image ?? attachment.fileWrapper?.regularFileContents
                      .flatMap(PlatformImage.init(data:)),
                  image.size.width > 0, image.size.height > 0
            else { return }

            let scale = min(1, min(box.width / image.size.width, box.height / image.size.height))
            attachment.bounds = CGRect(
                origin: .zero,
                size: CGSize(width: image.size.width * scale, height: image.size.height * scale)
            )
        }
    }
}
