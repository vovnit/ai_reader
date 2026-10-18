import Foundation

/// Writes a book read from another format — a PDF (`PDFImporter`), an FB2, a
/// text or an HTML file (`BookImporter`) — as an EPUB unpacked into its own
/// folder like any imported book, so it is read, searched and synced like any
/// other. The same as the Kindle app's `EpubWriter` and the web app's
/// `EpubWriter.js`, so the same file makes a book with the same identifier on
/// every device.
enum EPUBWriter {
    /// `urn:aireader:<kind>:<FNV-1a 64 of the file's bytes>`, `kind` being
    /// pdf, fb2, txt or html.
    static func identifier(_ kind: String, of data: Data) -> String {
        var hash: UInt64 = 0xcbf2_9ce4_8422_2325
        for byte in data { hash = (hash ^ UInt64(byte)) &* 0x100_0000_01b3 }
        let hex = String(hash, radix: 16)
        return "urn:aireader:\(kind):" + String(repeating: "0", count: 16 - hex.count) + hex
    }

    /// The language the chapters' prose is in, or nil when none stands out.
    static func proseLanguage(_ chapters: [ChapterDraft]) -> String? {
        TextLanguage.detect(in: chapters.flatMap(\.paragraphs).map(\.text).joined(separator: "\n"))
    }

    /// Writes the chapters into a new folder of the library, stamped with the
    /// time they are written. A language not known is written "und".
    static func write(
        title: String,
        author: String?,
        language: String?,
        identifier: String,
        chapters: [ChapterDraft]
    ) throws -> EPUBImporter.Result {
        let metadata = EPUBBuilder.Metadata(
            title: title,
            author: author,
            language: language ?? "und",
            identifier: identifier,
            modified: ISO8601DateFormatter().string(from: Date())
        )

        let folder = UUID().uuidString
        let directory = BookStorage.directory(named: folder)
        do {
            for file in EPUBBuilder.files(metadata, chapters: chapters) {
                let destination = directory.appending(path: file.path)
                try FileManager.default.createDirectory(
                    at: destination.deletingLastPathComponent(),
                    withIntermediateDirectories: true
                )
                try Data(file.contents.utf8).write(to: destination, options: .atomic)
            }
        } catch {
            try? FileManager.default.removeItem(at: directory)
            throw error
        }
        return EPUBImporter.Result(
            folder: folder,
            title: title,
            author: author,
            language: language,
            packagePath: EPUBBuilder.packagePath,
            coverPath: nil
        )
    }
}
