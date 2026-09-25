import Foundation

/// ABBYY Lingvo's DSL. Headwords start at column zero; the lines indented
/// under them are the article, marked up with `[tags]` this reader strips.
enum DSLDictionaryReader {
    static func read(
        _ source: DictionarySource,
        entry: (DictionaryImportEntry) throws -> Void
    ) throws -> DictionaryImportInfo {
        var info = DictionaryImportInfo(name: source.defaultName)
        var headwords: [String] = []
        var senses: [String] = []

        func flush() throws {
            defer { headwords = []; senses = [] }
            guard !headwords.isEmpty, !senses.isEmpty else { return }
            for headword in headwords {
                try entry(
                    DictionaryImportEntry(headword: headword, partOfSpeech: nil, senses: senses)
                )
            }
        }

        for line in try TextFile.lines(at: source.main) {
            if let directive = directive(in: line) {
                switch directive.0 {
                case "#NAME": info.name = directive.1
                case "#INDEX_LANGUAGE": info.targetLanguage = LanguageName.code(directive.1)
                case "#CONTENTS_LANGUAGE": info.definitionLanguage = LanguageName.code(directive.1)
                default: break
                }
                continue
            }

            let isIndented = line.first == "\t" || line.first == " "
            let text = strip(String(line)).trimmingCharacters(in: .whitespaces)

            if isIndented {
                if !text.isEmpty { senses.append(text) }
            } else {
                // A run of headwords shares the article indented below it.
                if !senses.isEmpty { try flush() }
                if !text.isEmpty { headwords.append(text) }
            }
        }
        try flush()
        return info
    }

    /// `#NAME "Big Dictionary"` split into its parts.
    private static func directive(in line: Substring) -> (String, String)? {
        guard line.hasPrefix("#") else { return nil }
        let parts = line.split(separator: " ", maxSplits: 1)
        guard let key = parts.first else { return nil }
        let value = parts.count > 1 ? String(parts[1]) : ""
        return (String(key).uppercased(), value.trimmingCharacters(in: CharacterSet(charactersIn: " \"\r")))
    }

    /// Removes DSL markup, keeping the words inside it.
    ///
    /// `[...]` are formatting tags, `{{...}}` are comments, and a backslash
    /// escapes the character after it.
    static func strip(_ line: String) -> String {
        var output = ""
        var depth = 0
        var index = line.startIndex

        while index < line.endIndex {
            let character = line[index]
            let next = line.index(after: index)

            if character == "\\", next < line.endIndex {
                output.append(line[next])
                index = line.index(after: next)
                continue
            }
            if character == "{", next < line.endIndex, line[next] == "{" {
                if let end = line.range(of: "}}", range: next..<line.endIndex) {
                    index = end.upperBound
                    continue
                }
            }
            switch character {
            case "[": depth += 1
            case "]": depth = max(0, depth - 1)
            case "{", "}": break  // headword parts that are shown but not indexed
            default: if depth == 0 { output.append(character) }
            }
            index = next
        }
        return output.replacingOccurrences(of: "\r", with: "")
    }
}
