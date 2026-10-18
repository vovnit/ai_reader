import Foundation
import PDFKit

/// Makes an EPUB of a PDF, unpacked into its own folder like any imported
/// book (`EPUBWriter`), so it is read, searched and synced the same way: the
/// PDF's text laid out again as chapters and paragraphs. PDFKit reads the
/// text; the rules that make it a book are the Kindle and web apps' too. A
/// scanned PDF has no text to take; ScanTool is for those.
enum PDFImporter {
    enum ImportError: LocalizedError {
        case unreadable
        case locked
        case noText

        var errorDescription: String? {
            switch self {
            case .unreadable: "The file is not a PDF."
            case .locked: "The PDF is protected by a password."
            case .noText: "The PDF has no text to read — it is probably a scan. ScanTool makes an EPUB of a scanned book from its OCR."
            }
        }
    }

    static func importPDF(at url: URL) throws -> EPUBImporter.Result {
        let data = try Data(contentsOf: url, options: .mappedIfSafe)
        guard let document = PDFDocument(data: data) else { throw ImportError.unreadable }
        if document.isLocked, !document.unlock(withPassword: "") { throw ImportError.locked }

        let pages = (0..<document.pageCount).map { document.page(at: $0).map(lines) ?? [] }
        let attributes = document.documentAttributes ?? [:]
        let title = (attributes[PDFDocumentAttribute.titleAttribute] as? String)?
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .nonEmpty ?? url.deletingPathExtension().lastPathComponent
        let author = (attributes[PDFDocumentAttribute.authorAttribute] as? String)?
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .nonEmpty
        let chapters = PDFLayout.chapters(pages, bookmarks: bookmarks(of: document), title: title)
        guard !chapters.isEmpty else { throw ImportError.noText }
        let result = try EPUBWriter.write(
            title: title,
            author: author,
            language: EPUBWriter.proseLanguage(chapters),
            identifier: EPUBWriter.identifier("pdf", of: data),
            chapters: chapters
        )
        // Without it the book still reads; only its page view is not offered.
        try? data.write(to: BookStorage.originalPDF(inFolder: result.folder))
        return result
    }

    /// A page's lines, in points from the top of the page. PDFKit may hand
    /// one line over in pieces, and not always left to right; pieces on one
    /// baseline are put together in order, with a space where there is a gap.
    private static func lines(of page: PDFPage) -> [PDFLine] {
        let box = page.bounds(for: .mediaBox)
        guard let selection = page.selection(for: box) else { return [] }
        var rows: [[PDFLine]] = []
        for piece in selection.selectionsByLine() {
            guard let text = piece.string?.trimmingCharacters(in: .newlines),
                  !text.unicodeScalars.allSatisfy(\.properties.isWhitespace)
            else { continue }
            // A box can run over two lines when one ends in a hyphen, and past
            // the page: measured from its middle, such a line sits between its
            // neighbours, and having no edges of its own it keeps the last
            // line's, rather than leaving a paragraph's gap or indent.
            let raw = piece.bounds(for: page)
            let bounds = raw.intersection(box)
            guard !bounds.isNull else { continue }
            let font = piece.attributedString?.attribute(.font, at: 0, effectiveRange: nil) as? PlatformFont
            var line = PDFLine(
                text: text,
                left: bounds.minX - box.minX,
                right: bounds.maxX - box.minX,
                y: box.maxY - bounds.midY,
                size: font.map { Double($0.pointSize) } ?? bounds.height
            )
            if raw.minX < box.minX || raw.maxX > box.maxX, let previous = rows.last?.last {
                line.left = previous.left
                line.right = previous.right
            }
            if let row = rows.lastIndex(where: { abs($0[0].y - line.y) <= max($0[0].size, line.size) * 0.5 }) {
                rows[row].append(line)
            } else {
                rows.append([line])
            }
        }
        return rows.map { row in
            let pieces = row.sorted { $0.left < $1.left }
            var line = pieces[0]
            for piece in pieces.dropFirst() {
                let spaced = line.text.unicodeScalars.last?.properties.isWhitespace == true
                    || piece.text.unicodeScalars.first?.properties.isWhitespace == true
                line.text += (piece.left - line.right > piece.size * 0.15 && !spaced ? " " : "") + piece.text
                line.right = max(line.right, piece.right)
                line.size = max(line.size, piece.size)
            }
            return line
        }
    }

    /// The top level of the outline; a lone entry holding the rest gives way
    /// to its children.
    private static func bookmarks(of document: PDFDocument) -> [PDFBookmark] {
        guard let root = document.outlineRoot else { return [] }
        let children = { (item: PDFOutline) in (0..<item.numberOfChildren).compactMap(item.child(at:)) }
        var items = children(root)
        if items.count == 1, items[0].numberOfChildren > 0 { items = children(items[0]) }
        return items.compactMap { item in
            let destination = item.destination ?? (item.action as? PDFActionGoTo)?.destination
            guard let page = destination?.page else { return nil }
            return PDFBookmark(title: item.label ?? "", page: document.index(for: page))
        }
    }
}

private extension String {
    var nonEmpty: String? { isEmpty ? nil : self }
}
