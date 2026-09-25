import Foundation

/// StarDict: an `.ifo` describing the dictionary, an `.idx` listing every
/// headword with where its article sits, and a `.dict` (often gzipped as
/// `.dict.dz`) holding the articles themselves.
enum StarDictReader {
    static func read(
        _ source: DictionarySource,
        entry: (DictionaryImportEntry) throws -> Void
    ) throws -> DictionaryImportInfo {
        let settings = try settings(of: source.main)
        guard let indexURL = source.companion(withSuffix: ".idx"),
              let bodyURL = source.companion(withSuffix: ".dict")
        else { throw DictionaryReadError.missingCompanions }

        let index = try Data(contentsOf: indexURL, options: .mappedIfSafe)
        var body = try Data(contentsOf: bodyURL, options: .mappedIfSafe)
        if body.starts(with: [0x1F, 0x8B]) {
            guard let inflated = Inflate.gzip(body) else {
                throw DictionaryReadError.unreadable(bodyURL.lastPathComponent)
            }
            body = inflated
        }

        let wide = settings["idxoffsetbits"] == "64"
        let sameType = settings["sametypesequence"]
        var cursor = index.startIndex

        while cursor < index.endIndex {
            guard let end = index[cursor...].firstIndex(of: 0) else { break }
            let word = String(decoding: index[cursor..<end], as: UTF8.self)
            var position = index.index(after: end)

            let offset = wide ? Int(number(index, at: &position, bytes: 8))
                              : Int(number(index, at: &position, bytes: 4))
            let size = Int(number(index, at: &position, bytes: 4))
            cursor = position

            guard offset >= 0, size > 0, offset + size <= body.count else { continue }
            let article = body[body.startIndex + offset ..< body.startIndex + offset + size]
            let senses = senses(in: Data(article), sameType: sameType)
            guard !word.isEmpty, !senses.isEmpty else { continue }
            try entry(DictionaryImportEntry(headword: word, partOfSpeech: nil, senses: senses))
        }

        return DictionaryImportInfo(
            name: settings["bookname"] ?? source.defaultName,
            targetLanguage: LanguageName.code(settings["lang"] ?? settings["sourcelang"]),
            definitionLanguage: LanguageName.code(settings["targetlang"])
        )
    }

    /// The `key=value` lines of an `.ifo`.
    private static func settings(of url: URL) throws -> [String: String] {
        var settings: [String: String] = [:]
        for line in try TextFile.lines(at: url) {
            let parts = line.split(separator: "=", maxSplits: 1)
            guard parts.count == 2 else { continue }
            settings[String(parts[0]).trimmingCharacters(in: .whitespaces).lowercased()] =
                String(parts[1]).trimmingCharacters(in: .whitespacesAndNewlines)
        }
        return settings
    }

    /// A big-endian integer, moving the cursor past it.
    private static func number(_ data: Data, at cursor: inout Data.Index, bytes: Int) -> UInt64 {
        var value: UInt64 = 0
        for _ in 0..<bytes {
            guard cursor < data.endIndex else { return 0 }
            value = value << 8 | UInt64(data[cursor])
            cursor = data.index(after: cursor)
        }
        return value
    }

    /// Splits an article into its senses.
    ///
    /// With `sametypesequence` the whole block is one field of a known type;
    /// without it, each field announces its own type first.
    private static func senses(in article: Data, sameType: String?) -> [String] {
        var texts: [String] = []

        if let sameType, sameType.count == 1 {
            texts = [text(article, type: Character(sameType))]
        } else {
            var cursor = article.startIndex
            while cursor < article.endIndex {
                let type = Character(UnicodeScalar(article[cursor]))
                cursor = article.index(after: cursor)
                if type.isUppercase {  // a length-prefixed, and so binary, field
                    let size = Int(number(article, at: &cursor, bytes: 4))
                    cursor = article.index(cursor, offsetBy: size, limitedBy: article.endIndex)
                        ?? article.endIndex
                } else {
                    let end = article[cursor...].firstIndex(of: 0) ?? article.endIndex
                    texts.append(text(article[cursor..<end], type: type))
                    cursor = end < article.endIndex ? article.index(after: end) : end
                }
            }
        }

        return texts.flatMap { $0.split(whereSeparator: \.isNewline) }
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
    }

    /// Reads one field, unwrapping the markup its type implies.
    private static func text(_ field: some DataProtocol, type: Character) -> String {
        let raw = String(decoding: Data(field), as: UTF8.self)
        switch type {
        case "h", "x", "g": return Markup.stripTags(from: raw)
        case "m", "l", "t", "y", "k", "w": return raw
        default: return ""
        }
    }
}

/// Enough of an HTML/XDXF stripper for dictionary articles.
enum Markup {
    static func stripTags(from text: String) -> String {
        var output = ""
        var depth = 0
        for character in text {
            switch character {
            case "<": depth += 1
            case ">": depth = max(0, depth - 1)
            case _ where depth == 0: output.append(character)
            default: break
            }
        }
        return output
            .replacingOccurrences(of: "&nbsp;", with: " ")
            .replacingOccurrences(of: "&lt;", with: "<")
            .replacingOccurrences(of: "&gt;", with: ">")
            .replacingOccurrences(of: "&quot;", with: "\"")
            .replacingOccurrences(of: "&amp;", with: "&")
    }
}
