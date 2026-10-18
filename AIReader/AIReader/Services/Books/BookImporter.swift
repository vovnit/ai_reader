import Foundation
import UniformTypeIdentifiers

/// Makes an EPUB of an FB2 (bare or zipped), a plain-text file or an HTML
/// page, unpacked into its own folder like any imported book (`EPUBWriter`),
/// so it is read, searched and synced like any other. The file's name says
/// which it is, or for a name not known, its type. The Kindle app's
/// `BookImporter` and the web app's `BookImporter.js` take files in by the
/// same rules, so a file makes the same book, with the same chapters, on
/// every device. A PDF is `PDFImporter`'s.
enum BookImporter {
    enum ImportError: LocalizedError {
        case unknownFormat
        case noFB2InZIP
        case unreadableFB2InZIP
        case noText

        var errorDescription: String? {
            switch self {
            case .unknownFormat: "Not a book this app can read."
            case .noFB2InZIP: "The ZIP holds no FB2 book."
            case .unreadableFB2InZIP: "The FB2 book in the ZIP cannot be read."
            case .noText: "The file has no text to read."
            }
        }
    }

    private enum Format {
        case fb2, zippedFB2, text, html
    }

    /// Name endings, lowercased, and the format each names. ".fb2.zip" is one
    /// ending, so it leaves the rest of the name as the title, and any ZIP is
    /// looked into for an FB2, the way FB2 books are shared, or else an EPUB.
    private static let endings: [(ending: String, format: Format)] = [
        (".fb2", .fb2), (".fb2.zip", .zippedFB2), (".fbz", .zippedFB2), (".zip", .zippedFB2),
        (".txt", .text), (".html", .html), (".htm", .html), (".xhtml", .html),
    ]

    /// Whether the file's name, in any case, ends in one of the endings above
    /// (its whole name, since "x.fb2.zip" has the extension "zip"), or its
    /// type is text or HTML.
    static func reads(_ url: URL) -> Bool {
        named(url) != nil
    }

    /// The EPUB of a file `reads`.
    static func importBook(at url: URL) throws -> EPUBImporter.Result {
        guard let file = named(url) else { throw ImportError.unknownFormat }
        let data = try Data(contentsOf: url, options: .mappedIfSafe)
        let title = file.title
        // A ZIP is read for its FB2, unless it is an EPUB.
        var fb2 = data
        if file.format == .zippedFB2 {
            guard let zipped = try zippedFB2(data) else { return try EPUBImporter.unpack(epubAt: url) }
            fb2 = zipped
        }
        let (kind, draft) = switch file.format {
        case .fb2, .zippedFB2: ("fb2", FB2Book.read(declaredText(fb2), fallbackTitle: title))
        // A text file names no encoding; Charset tells it from the bytes.
        case .text: ("txt", TextBook.read(Charset.decode(data), fallbackTitle: title))
        case .html: ("html", HTMLBook.read(declaredText(data), fallbackTitle: title))
        }
        guard !draft.chapters.isEmpty else { throw ImportError.noText }
        return try EPUBWriter.write(
            title: draft.title,
            author: draft.author,
            language: draft.language ?? EPUBWriter.proseLanguage(draft.chapters),
            identifier: EPUBWriter.identifier(kind, of: data),
            chapters: draft.chapters
        )
    }

    /// The format a file's name says it is in, and the name without its
    /// ending, the title of a book that does not name itself. A name not
    /// known is read by its type when that is text or HTML, as the picker
    /// offers them, and is the title whole.
    private static func named(_ url: URL) -> (format: Format, title: String)? {
        let name = url.lastPathComponent
        let lowered = name.lowercased()
        if let match = endings.first(where: { lowered.hasSuffix($0.ending) }) {
            return (match.format, String(name.dropLast(match.ending.count)))
        }
        let type = try? url.resourceValues(forKeys: [.contentTypeKey]).contentType
        if type?.conforms(to: .html) == true { return (.html, name) }
        if type?.conforms(to: .plainText) == true { return (.text, name) }
        return nil
    }

    /// A file's text in the encoding it names, if it names one.
    private static func declaredText(_ data: Data) -> String {
        Charset.decode(data, declared: Charset.declared(in: data))
    }

    /// Whether a ZIP entry is an FB2, and not the resource fork a Mac adds
    /// beside a file ("__MACOSX/", "._"), which ends the same. Its bytes are
    /// compared, as the other apps compare them, not its characters.
    private static func isFB2(_ entry: String) -> Bool {
        let bytes = Array(entry.utf8)
        let base = bytes[(bytes.lastIndex(of: UInt8(ascii: "/")).map { $0 + 1 } ?? 0)...]
        return entry.lowercased().utf8.suffix(4).elementsEqual(".fb2".utf8)
            && !bytes.starts(with: "__MACOSX/".utf8) && !base.starts(with: "._".utf8)
    }

    /// The bytes of the first FB2 in a ZIP, in the archive's own order, or
    /// nil for a ZIP that is an EPUB.
    private static func zippedFB2(_ data: Data) throws -> Data? {
        let archive = try ZIPArchive(data: data)
        guard let path = archive.paths.first(where: isFB2) else {
            // An EPUB is a ZIP too, and one renamed .zip is still a book.
            let container = archive.paths.contains { $0.caseInsensitiveCompare("META-INF/container.xml") == .orderedSame }
            if container { return nil }
            throw ImportError.noFB2InZIP
        }
        do {
            return try archive.contents(of: path)
        } catch {
            throw ImportError.unreadableFB2InZIP
        }
    }
}
