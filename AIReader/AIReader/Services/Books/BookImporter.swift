import Foundation

/// Makes an EPUB of an FB2 (bare or zipped), a plain-text file or an HTML
/// page, unpacked into its own folder like any imported book (`EPUBWriter`),
/// so it is read, searched and synced like any other. The file's name says
/// which it is. The Kindle app's `BookImporter` and the web app's
/// `BookImporter.js` take files in by the same rules, so a file makes the
/// same book, with the same chapters, on every device. A PDF is
/// `PDFImporter`'s.
enum BookImporter {
    enum ImportError: LocalizedError {
        case unknownFormat
        case noFB2InZIP
        case noText

        var errorDescription: String? {
            switch self {
            case .unknownFormat: "Not a book this app can read."
            case .noFB2InZIP: "The ZIP holds no FB2 book."
            case .noText: "The file has no text to read."
            }
        }
    }

    private enum Format {
        case fb2, zippedFB2, text, html
    }

    /// Name endings, lowercased, and the format each names. ".fb2.zip" is one
    /// ending, so it leaves the rest of the name as the title, and any ZIP is
    /// looked into for an FB2, the way FB2 books are shared.
    private static let endings: [(ending: String, format: Format)] = [
        (".fb2", .fb2), (".fb2.zip", .zippedFB2), (".fbz", .zippedFB2), (".zip", .zippedFB2),
        (".txt", .text), (".html", .html), (".htm", .html), (".xhtml", .html),
    ]

    /// Whether the file's name, in any case, ends in one of the endings above.
    /// Its whole name, since "x.fb2.zip" has the extension "zip".
    static func reads(_ url: URL) -> Bool {
        named(url) != nil
    }

    /// The EPUB of a file whose name `reads`.
    static func importBook(at url: URL) throws -> EPUBImporter.Result {
        guard let file = named(url) else { throw ImportError.unknownFormat }
        let data = try Data(contentsOf: url, options: .mappedIfSafe)
        let title = file.title
        let (kind, draft) = switch file.format {
        case .fb2: ("fb2", FB2Book.read(declaredText(data), fallbackTitle: title))
        case .zippedFB2: ("fb2", FB2Book.read(declaredText(try zippedFB2(data)), fallbackTitle: title))
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
    /// ending, the title of a book that does not name itself.
    private static func named(_ url: URL) -> (format: Format, title: String)? {
        let name = url.lastPathComponent
        let lowered = name.lowercased()
        guard let match = endings.first(where: { lowered.hasSuffix($0.ending) }) else { return nil }
        return (match.format, String(name.dropLast(match.ending.count)))
    }

    /// A file's text in the encoding it names, if it names one.
    private static func declaredText(_ data: Data) -> String {
        Charset.decode(data, declared: Charset.declared(in: data))
    }

    /// The bytes of the first FB2 in a ZIP, in the archive's own order.
    private static func zippedFB2(_ data: Data) throws -> Data {
        let archive = try ZIPArchive(data: data)
        guard let path = archive.paths.first(where: { $0.lowercased().hasSuffix(".fb2") }) else {
            throw ImportError.noFB2InZIP
        }
        return try archive.contents(of: path)
    }
}
