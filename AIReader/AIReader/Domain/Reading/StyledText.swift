import Foundation

#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// Applies a `ReadingStyle` to a book that has already been loaded, so changing
/// the font does not mean parsing the EPUB again.
enum StyledText {
    static func apply(_ style: ReadingStyle, to text: NSAttributedString) -> NSAttributedString {
        let styled = NSMutableAttributedString(attributedString: text)
        let whole = NSRange(location: 0, length: styled.length)

        styled.enumerateAttribute(.font, in: whole) { value, range, _ in
            let base = (value as? PlatformFont) ?? .preferredFont(forTextStyle: .body)
            styled.addAttribute(.font, value: font(base, style: style), range: range)
        }

        let paragraph = NSMutableParagraphStyle()
        paragraph.lineSpacing = style.lineSpacing
        styled.enumerateAttribute(.paragraphStyle, in: whole) { value, range, _ in
            // Keep the book's own alignment and indents; change only leading.
            let merged = ((value as? NSParagraphStyle)?.mutableCopy() as? NSMutableParagraphStyle)
                ?? paragraph
            merged.lineSpacing = style.lineSpacing
            styled.addAttribute(.paragraphStyle, value: merged, range: range)
        }

        styled.addAttribute(.foregroundColor, value: PlatformColor.label, range: whole)
        return styled
    }

    private static func font(_ base: PlatformFont, style: ReadingStyle) -> PlatformFont {
        let size = base.pointSize * style.scale
        guard let name = style.fontName else { return base.withSize(size) }

        // Preserve bold and italic from the book while swapping the family.
        #if canImport(UIKit)
        let descriptor = UIFontDescriptor(name: name, size: size)
            .withSymbolicTraits(base.fontDescriptor.symbolicTraits)
        return UIFont(descriptor: descriptor ?? UIFontDescriptor(name: name, size: size), size: size)
        #else
        let descriptor = NSFontDescriptor(name: name, size: size)
            .withSymbolicTraits(base.fontDescriptor.symbolicTraits)
        return NSFont(descriptor: descriptor, size: size) ?? base.withSize(size)
        #endif
    }
}
