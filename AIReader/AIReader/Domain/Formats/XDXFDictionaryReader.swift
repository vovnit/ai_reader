import Foundation

/// XDXF, the open XML dictionary format. Articles are `<ar>` elements holding
/// one or more `<k>` headwords and the definition text around them.
enum XDXFDictionaryReader {
    static func read(
        _ source: DictionarySource,
        entry: (DictionaryImportEntry) throws -> Void
    ) throws -> DictionaryImportInfo {
        // The delegate lets go of `entry` as soon as parsing stops, so it does
        // not outlive this call.
        try withoutActuallyEscaping(entry) { entry in
            let delegate = Articles(source: source, entry: entry)
            // Parsing from the URL rather than from `Data` keeps a large
            // dictionary off the heap.
            guard let parser = XMLParser(contentsOf: source.main) else {
                throw DictionaryReadError.unreadable(source.main.lastPathComponent)
            }
            parser.delegate = delegate
            parser.parse()
            try delegate.thrown.map { throw $0 }
            return delegate.info
        }
    }

    /// Collects one `<ar>` at a time and reports it.
    private final class Articles: NSObject, XMLParserDelegate {
        var info: DictionaryImportInfo
        var thrown: (any Error)?

        private let entry: (DictionaryImportEntry) throws -> Void
        private var headwords: [String] = []
        private var body = ""
        private var text = ""
        private var inArticle = false
        private var inKey = false

        init(source: DictionarySource, entry: @escaping (DictionaryImportEntry) throws -> Void) {
            self.info = DictionaryImportInfo(name: source.defaultName)
            self.entry = entry
        }

        func parser(
            _ parser: XMLParser,
            didStartElement element: String,
            namespaceURI: String?,
            qualifiedName: String?,
            attributes: [String: String]
        ) {
            switch element.lowercased() {
            case "xdxf":
                info.targetLanguage = LanguageName.code(attributes["lang_from"])
                info.definitionLanguage = LanguageName.code(attributes["lang_to"])
            case "ar":
                inArticle = true
                headwords = []
                body = ""
            case "k" where inArticle:
                inKey = true
                text = ""
            default:
                break
            }
            if element.lowercased() == "full_name" { text = "" }
        }

        func parser(_ parser: XMLParser, foundCharacters characters: String) {
            text += characters
            if inArticle, !inKey { body += characters }
        }

        func parser(
            _ parser: XMLParser,
            didEndElement element: String,
            namespaceURI: String?,
            qualifiedName: String?
        ) {
            switch element.lowercased() {
            case "full_name":
                let name = text.trimmingCharacters(in: .whitespacesAndNewlines)
                if !name.isEmpty { info.name = name }
            case "k":
                inKey = false
                let headword = text.trimmingCharacters(in: .whitespacesAndNewlines)
                if !headword.isEmpty { headwords.append(headword) }
            case "ar":
                inArticle = false
                report(parser)
            default:
                break
            }
            text = ""
        }

        private func report(_ parser: XMLParser) {
            let senses = body
                .split(whereSeparator: \.isNewline)
                .map { $0.trimmingCharacters(in: .whitespaces) }
                .filter { !$0.isEmpty }
            guard !senses.isEmpty else { return }

            do {
                for headword in headwords {
                    try entry(
                        DictionaryImportEntry(headword: headword, partOfSpeech: nil, senses: senses)
                    )
                }
            } catch {
                thrown = error
                parser.abortParsing()
            }
        }
    }
}
