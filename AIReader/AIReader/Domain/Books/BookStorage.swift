import Foundation

/// Where unpacked books live on disk. Books are addressed by folder name rather
/// than absolute path, because the app container path is not stable across
/// installs.
enum BookStorage {
    static var root: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        return base.appending(path: "Books", directoryHint: .isDirectory)
    }

    static func directory(named folder: String) -> URL {
        root.appending(path: folder, directoryHint: .isDirectory)
    }

    /// The PDF a book was made from, kept beside its EPUB on the device that
    /// imported it, for the page view; it is never sent to another device.
    static let originalPDFName = "original.pdf"

    static func originalPDF(inFolder folder: String) -> URL {
        directory(named: folder).appending(path: originalPDFName)
    }
}
