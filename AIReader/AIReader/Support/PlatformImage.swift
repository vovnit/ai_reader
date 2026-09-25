import SwiftUI

#if canImport(UIKit)
import UIKit
typealias PlatformImage = UIImage
#else
import AppKit
typealias PlatformImage = NSImage
#endif

extension Image {
    /// Loads an image from a file on disk.
    init?(contentsOf url: URL) {
        guard let data = try? Data(contentsOf: url), let image = PlatformImage(data: data) else {
            return nil
        }
        #if canImport(UIKit)
        self.init(uiImage: image)
        #else
        self.init(nsImage: image)
        #endif
    }
}
