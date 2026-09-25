import Foundation

/// One article, in the shape every reader produces and the writer consumes.
struct DictionaryImportEntry: Equatable, Sendable {
    var headword: String
    var partOfSpeech: String?
    var senses: [String]
}

/// What a dictionary file says about itself.
struct DictionaryImportInfo: Equatable, Sendable {
    var name: String
    var targetLanguage: String?
    var definitionLanguage: String?
}

/// What can go wrong reading a dictionary file.
enum DictionaryReadError: LocalizedError {
    case unrecognized(String)
    case unreadable(String)
    case missingCompanions

    var errorDescription: String? {
        switch self {
        case let .unrecognized(name):
            "“\(name)” is not a dictionary format this app reads."
        case let .unreadable(name):
            "“\(name)” could not be read."
        case .missingCompanions:
            "A StarDict dictionary needs its .idx and .dict files picked alongside the .ifo."
        }
    }
}

/// The dictionary file formats the app can read.
enum DictionaryFormat: Equatable, Sendable {
    /// A pack this app already understands, added as it is.
    case native
    /// One headword and its definition per line, separated by tabs or commas.
    case delimited
    /// The open XDXF XML format.
    case xdxf
    /// ABBYY Lingvo's DSL, plain or gzipped.
    case dsl
    /// StarDict: an `.ifo` describing sibling `.idx` and `.dict` files.
    case starDict

    static let all: [DictionaryFormat] = [.native, .delimited, .xdxf, .dsl, .starDict]

    /// How the format is named to the reader.
    var label: String {
        switch self {
        case .native: "AIReader pack"
        case .delimited: "tab- or comma-separated"
        case .xdxf: "XDXF"
        case .dsl: "Lingvo DSL"
        case .starDict: "StarDict"
        }
    }
}

/// One dictionary to import: the file that names it, plus the files that
/// belong with it. Only StarDict spreads itself over several files.
struct DictionarySource: Equatable, Sendable {
    var format: DictionaryFormat
    var main: URL
    var companions: [URL] = []

    /// The companion whose name ends in `suffix`, ignoring a trailing `.dz`.
    func companion(withSuffix suffix: String) -> URL? {
        companions.first { url in
            var name = url.lastPathComponent.lowercased()
            if name.hasSuffix(".dz") { name = String(name.dropLast(3)) }
            return name.hasSuffix(suffix)
        }
    }

    /// The name to fall back on when the file itself carries none.
    var defaultName: String {
        var name = main.deletingPathExtension().lastPathComponent
        if name.lowercased().hasSuffix(".dsl") { name = String(name.dropLast(4)) }
        return name
    }
}

extension DictionaryFormat {
    /// Sorts a set of picked files into the dictionaries they make up.
    /// StarDict's `.idx` and `.dict` files join the `.ifo` they sit beside.
    static func sources(from urls: [URL]) -> [DictionarySource] {
        // `.idx` and `.dict` have no format of their own, so they drop out here
        // and are picked back up as companions of the `.ifo` beside them.
        var sources = urls.compactMap { url in
            of(url).map { DictionarySource(format: $0, main: url) }
        }
        for (index, source) in sources.enumerated() where source.format == .starDict {
            let base = stem(of: source.main)
            sources[index].companions = urls.filter { $0 != source.main && stem(of: $0) == base }
        }
        return sources
    }

    /// The format of a single file, or nil when it is a companion or unknown.
    static func of(_ url: URL) -> DictionaryFormat? {
        switch fileExtension(of: url) {
        case "sqlite3", "sqlite", "db": .native
        case "tsv", "csv", "txt": .delimited
        case "xdxf": .xdxf
        case "dsl": .dsl
        case "ifo": .starDict
        case "xml": sniff(url) == .xdxf ? .xdxf : nil
        default: sniff(url)
        }
    }

    /// The extension, seeing through a trailing `.dz` or `.gz`.
    private static func fileExtension(of url: URL) -> String {
        var name = url.lastPathComponent.lowercased()
        for wrapper in [".dz", ".gz"] where name.hasSuffix(wrapper) {
            name = String(name.dropLast(wrapper.count))
        }
        return (name as NSString).pathExtension
    }

    /// The file name without its extension, seeing through `.dz`.
    private static func stem(of url: URL) -> String {
        var name = url.lastPathComponent
        for wrapper in [".dz", ".gz"] where name.lowercased().hasSuffix(wrapper) {
            name = String(name.dropLast(wrapper.count))
        }
        return (name as NSString).deletingPathExtension
    }

    /// Reads the first bytes for formats whose extension is unhelpful.
    private static func sniff(_ url: URL) -> DictionaryFormat? {
        guard let handle = try? FileHandle(forReadingFrom: url) else { return nil }
        defer { try? handle.close() }
        guard let head = try? handle.read(upToCount: 512) else { return nil }

        if head.starts(with: Array("SQLite format 3".utf8)) { return .native }
        let text = String(decoding: head, as: UTF8.self).lowercased()
        if text.contains("<xdxf") { return .xdxf }
        if text.hasPrefix("#name") { return .dsl }
        return nil
    }
}

/// Turns whatever a dictionary calls its language into a code `Locale` knows.
enum LanguageName {
    static func code(_ text: String?) -> String? {
        guard let text = text?.trimmingCharacters(in: .whitespacesAndNewlines).lowercased(),
              !text.isEmpty
        else { return nil }

        if text.count == 2 { return text }
        if text.count == 3, let alpha2 = Locale.LanguageCode(text).identifier(.alpha2) {
            return alpha2
        }
        // DSL writes the language out in English: "French", "Russian".
        let english = Locale(identifier: "en")
        return Locale.LanguageCode.isoLanguageCodes.first {
            english.localizedString(forLanguageCode: $0.identifier)?.lowercased() == text
        }?.identifier(.alpha2)
    }
}
