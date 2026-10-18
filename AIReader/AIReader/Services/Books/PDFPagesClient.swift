import ComposableArchitecture
import Foundation
import PDFKit

/// The text of each page of the PDF a book was made from, as PDFKit reads
/// it, for the page view to find a tapped word in.
@DependencyClient
struct PDFPagesClient: Sendable {
    var pageTexts: @Sendable (_ pdf: URL) async throws -> [String]
}

extension PDFPagesClient: DependencyKey {
    static let liveValue = Self(pageTexts: { url in
        guard let document = PDFDocument(url: url) else { throw PDFImporter.ImportError.unreadable }
        return (0..<document.pageCount).map { document.page(at: $0)?.string ?? "" }
    })

    static let testValue = Self()
}

extension DependencyValues {
    var pdfPagesClient: PDFPagesClient {
        get { self[PDFPagesClient.self] }
        set { self[PDFPagesClient.self] = newValue }
    }
}
