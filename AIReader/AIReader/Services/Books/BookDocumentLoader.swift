import Foundation

#if canImport(UIKit)
import UIKit
#else
import AppKit
#endif

/// Renders an unpacked EPUB into a single attributed string, images included,
/// with its table of contents pointed into it.
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
        let navigation = Navigation(package, root: root, packageDirectory: packageDirectory)

        let book = NSMutableAttributedString()
        // One range per reading-order item that has something to show; an
        // empty item is skipped and not counted, as the Kindle app does, so
        // chapter numbers agree between the two.
        var chapters: [NSRange] = []
        // Which chapter each file became and where its anchors are, and each
        // chapter's first heading, for the table of contents.
        var chapterByPath: [String: Int] = [:]
        var anchorsByPath: [String: [String: Int]] = [:]
        var headings: [String?] = []
        for item in package.readingOrder {
            let path = EPUBImporter.resolve(item.href, relativeTo: packageDirectory)
            let ids = navigation.fragments[path] ?? []
            let start = book.length
            if let (chapter, anchors) = chapter(at: root.appending(path: path), anchoring: ids),
               !chapter.string.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                chapterByPath[path] = chapters.count
                anchorsByPath[path] = Dictionary(anchors.map { (ids[$0.key], $0.value) }) { first, _ in first }
                headings.append(firstHeading(in: chapter))
                book.append(chapter)
                chapters.append(NSRange(location: start, length: book.length - start))
                book.append(NSAttributedString(string: "\n\n"))
            }
            await Task.yield()  // keep the app responsive while long books load
        }

        guard book.length > 0 else { throw LoadError.empty }
        // An entry for a file dropped as blank, or never in the spine, is left
        // out; a book without a table of contents gets an entry a chapter.
        var contents = navigation.entries.compactMap { entry -> ContentsEntry? in
            guard let chapter = chapterByPath[entry.path] else { return nil }
            let offset = anchorsByPath[entry.path]?[entry.fragment] ?? 0
            return ContentsEntry(title: entry.title, depth: entry.depth, chapter: chapter, offset: chapters[chapter].location + offset)
        }
        if contents.isEmpty {
            contents = chapters.indices.map {
                ContentsEntry(title: headings[$0] ?? "Chapter \($0 + 1)", depth: 0, chapter: $0, offset: chapters[$0].location)
            }
        }
        return BookDocument(text: book, chapters: chapters, contents: contents)
    }

    /// The table of contents, its hrefs resolved to paths in the book, and
    /// the places inside files it links to.
    private struct Navigation {
        struct Entry {
            let title: String
            let depth: Int
            let path: String
            let fragment: String
        }

        var entries: [Entry] = []
        /// The ids linked to in each file, in order, each once.
        var fragments: [String: [String]] = [:]

        init(_ package: EPUBPackage, root: URL, packageDirectory: String) {
            guard let item = package.navigationItem else { return }
            let navigationPath = EPUBImporter.resolve(item.href, relativeTo: packageDirectory)
            guard let data = try? Data(contentsOf: root.appending(path: navigationPath)) else { return }
            let directory = (navigationPath as NSString).deletingLastPathComponent
            for entry in EPUBNavigation.parse(data) {
                let parts = entry.href.split(separator: "#", maxSplits: 1, omittingEmptySubsequences: false)
                let file = String(parts[0]).removingPercentEncoding ?? String(parts[0])
                let fragment = parts.count > 1 ? String(parts[1]) : ""
                let path = EPUBImporter.resolve(file, relativeTo: directory)
                entries.append(Entry(title: entry.title, depth: entry.depth, path: path, fragment: fragment))
                if !fragment.isEmpty, !(fragments[path] ?? []).contains(fragment) {
                    fragments[path, default: []].append(fragment)
                }
            }
        }
    }

    /// A chapter's text, and where each of `ids` begins in it, by its place
    /// in `ids`.
    private static func chapter(at url: URL, anchoring ids: [String]) -> (NSMutableAttributedString, [Int: Int])? {
        guard let markup = try? String(contentsOf: url, encoding: .utf8) else { return nil }
        let directory = url.deletingLastPathComponent()
        let marked = ids.isEmpty ? markup : ChapterAnchors.mark(ids, in: markup)
        guard let text = try? NSMutableAttributedString(
            data: Data(absoluteImageSources(in: marked, relativeTo: directory).utf8),
            options: [
                .documentType: NSAttributedString.DocumentType.html,
                .characterEncoding: String.Encoding.utf8.rawValue
            ],
            documentAttributes: nil
        ) else { return nil }
        return (text, ids.isEmpty ? [:] : ChapterAnchors.locate(in: text))
    }

    /// The chapter's first heading, on one line: the first run set larger
    /// than the text, as the HTML import sets `h1` to `h3`.
    private static func firstHeading(in chapter: NSAttributedString) -> String? {
        var heading: String?
        chapter.enumerateAttribute(.font, in: NSRange(location: 0, length: chapter.length)) { value, range, stop in
            guard let font = value as? PlatformFont, font.pointSize > 13 else { return }
            let text = (chapter.string as NSString).substring(with: range)
            let line = text.components(separatedBy: .newlines).first?.trimmingCharacters(in: .whitespaces) ?? ""
            guard !line.isEmpty else { return }
            heading = line
            stop.pointee = true
        }
        return heading
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
