import Foundation

/// Word lists: one headword and its definition per line, separated by tabs or
/// by commas. The plainest thing a reader is likely to have made themselves.
enum DelimitedDictionaryReader {
    static func read(
        _ source: DictionarySource,
        entry: (DictionaryImportEntry) throws -> Void
    ) throws -> DictionaryImportInfo {
        let lines = try TextFile.lines(at: source.main)
        let separator = separator(in: lines)

        for line in lines {
            let text = line.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !text.isEmpty, !text.hasPrefix("#") else { continue }

            let fields = fields(of: text, separator: separator)
            guard let headword = fields.first, fields.count > 1 else { continue }
            try entry(
                DictionaryImportEntry(
                    headword: headword,
                    partOfSpeech: nil,
                    senses: Array(fields.dropFirst())
                )
            )
        }
        return DictionaryImportInfo(name: source.defaultName)
    }

    /// Tabs when the file uses any, commas otherwise.
    private static func separator(in lines: [Substring]) -> Character {
        lines.prefix(20).contains { $0.contains("\t") } ? "\t" : ","
    }

    /// Splits a line, honouring the double quotes a spreadsheet export adds.
    private static func fields(of line: String, separator: Character) -> [String] {
        var fields: [String] = []
        var field = ""
        var quoted = false

        for character in line {
            switch character {
            case "\"": quoted.toggle()
            case separator where !quoted:
                fields.append(field)
                field = ""
            default: field.append(character)
            }
        }
        fields.append(field)
        return fields.map { $0.trimmingCharacters(in: .whitespaces) }.filter { !$0.isEmpty }
    }
}
