#if canImport(UIKit)
import UIKit
typealias PlatformFont = UIFont
typealias PlatformColor = UIColor
#else
import AppKit
typealias PlatformFont = NSFont
typealias PlatformColor = NSColor

extension NSColor {
    /// UIKit's name for the primary text colour, so styling reads the same on
    /// both platforms.
    static var label: NSColor { .labelColor }
}
#endif
