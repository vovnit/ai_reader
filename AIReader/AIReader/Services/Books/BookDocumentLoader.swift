import Foundation

#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// Renders an unpacked EPUB into a single attributed string, images included.
@MainActor
enum BookDocumentLoader {
    enum LoadError: LocalizedError {
        case empty

        var errorDescription: String? { "The book has no readable content." }
    }

    static func load(folder: String, packagePath: String) async throws -> BookDocument {
        let root = BookStorage.directory(named: folder)
        let package = EPUBPackage.parse(try Data(contentsOf: root.appending(path: packagePath)))
        let packageDirectory = (packagePath as NSString).deletingLastPathComponent

        let book = NSMutableAttributedString()
        // One range per reading-order item that has something to show; an
        // empty item is skipped and not counted, as the Kindle app does, so
        // chapter numbers agree between the two.
        var chapters: [NSRange] = []
        for item in package.readingOrder {
            let path = EPUBImporter.resolve(item.href, relativeTo: packageDirectory)
            let url = root.appending(path: path)
            let start = book.length
            if let chapter = chapter(at: url), !chapter.string.trimmingCharacters(
                in: .whitespacesAndNewlines
            ).isEmpty {
                book.append(chapter)
                chapters.append(NSRange(location: start, length: book.length - start))
                book.append(NSAttributedString(string: "\n\n"))
            }
            await Task.yield()  // keep the app responsive while long books load
        }

        guard book.length > 0 else { throw LoadError.empty }
        return BookDocument(text: book, chapters: chapters)
    }

    private static func chapter(at url: URL) -> NSAttributedString? {
        guard let markup = try? String(contentsOf: url, encoding: .utf8) else { return nil }
        let directory = url.deletingLastPathComponent()
        return try? NSAttributedString(
            data: Data(absoluteImageSources(in: markup, relativeTo: directory).utf8),
            options: [
                .documentType: NSAttributedString.DocumentType.html,
                .characterEncoding: String.Encoding.utf8.rawValue
            ],
            documentAttributes: nil
        )
    }

    /// The HTML importer loads images itself, but only when it can resolve
    /// them, so relative sources are rewritten to absolute file URLs.
    private static func absoluteImageSources(in markup: String, relativeTo directory: URL) -> String {
        let pattern = /(<img\b[^>]*?\bsrc\s*=\s*")([^"]+)(")/
        return markup.replacing(pattern) { match in
            let source = String(match.2)
            guard !source.contains("://"), !source.hasPrefix("data:") else { return match.0 }
            let resolved = URL(fileURLWithPath: source.removingPercentEncoding ?? source, relativeTo: directory)
            return "\(match.1)\(resolved.standardizedFileURL.absoluteString)\(match.3)"
        }
    }
}
