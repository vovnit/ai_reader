import Foundation

/// Walks an XML document and reports elements to closures, so small parsers can
/// be written without a delegate class each time.
final class XMLScanner: NSObject, XMLParserDelegate {
    typealias ElementHandler = (_ name: String, _ attributes: [String: String]) -> Void
    typealias TextHandler = (_ name: String, _ text: String) -> Void

    private let onElement: ElementHandler
    private let onText: TextHandler
    private var openElement = ""
    private var text = ""

    private init(onElement: @escaping ElementHandler, onText: @escaping TextHandler) {
        self.onElement = onElement
        self.onText = onText
    }

    /// Parses `data`, calling `onElement` for every start tag and `onText` with
    /// the accumulated text of every element that contains any. Element names
    /// are reported without their namespace prefix.
    static func scan(
        _ data: Data,
        onElement: @escaping ElementHandler,
        onText: @escaping TextHandler = { _, _ in }
    ) {
        let scanner = XMLScanner(onElement: onElement, onText: onText)
        let parser = XMLParser(data: data)
        parser.delegate = scanner
        parser.parse()
    }

    func parser(
        _ parser: XMLParser,
        didStartElement element: String,
        namespaceURI: String?,
        qualifiedName: String?,
        attributes: [String: String]
    ) {
        openElement = Self.localName(element)
        text = ""
        onElement(openElement, attributes)
    }

    func parser(_ parser: XMLParser, foundCharacters characters: String) {
        text += characters
    }

    func parser(
        _ parser: XMLParser,
        didEndElement element: String,
        namespaceURI: String?,
        qualifiedName: String?
    ) {
        let name = Self.localName(element)
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        if name == openElement, !trimmed.isEmpty {
            onText(name, trimmed)
        }
        text = ""
    }

    private static func localName(_ element: String) -> String {
        element.split(separator: ":").last.map(String.init) ?? element
    }
}
