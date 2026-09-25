import Foundation

/// Reads a text file whose encoding is only known from its first bytes.
/// Dictionary sources arrive as UTF-8, as UTF-16 (Lingvo's habit), and
/// gzipped (the `.dz` files that ship beside StarDict and DSL dictionaries).
enum TextFile {
    static func text(at url: URL) throws -> String {
        var data = try Data(contentsOf: url, options: .mappedIfSafe)
        if data.starts(with: [0x1F, 0x8B]), let inflated = Inflate.gzip(data) {
            data = inflated
        }
        return decode(data)
    }

    static func lines(at url: URL) throws -> [Substring] {
        try text(at: url).split(separator: "\n", omittingEmptySubsequences: false)
    }

    static func decode(_ data: Data) -> String {
        if data.starts(with: [0xFF, 0xFE]) {
            return string(data.dropFirst(2), .utf16LittleEndian)
        }
        if data.starts(with: [0xFE, 0xFF]) {
            return string(data.dropFirst(2), .utf16BigEndian)
        }
        if data.starts(with: [0xEF, 0xBB, 0xBF]) {
            return string(data.dropFirst(3), .utf8)
        }
        // No mark: UTF-8 unless it does not decode, and then the two encodings
        // dictionary files otherwise turn up in.
        return String(data: data, encoding: .utf8)
            ?? String(data: data, encoding: .utf16LittleEndian)
            ?? string(data, .isoLatin1)
    }

    private static func string(_ data: some DataProtocol, _ encoding: String.Encoding) -> String {
        String(data: Data(data), encoding: encoding) ?? ""
    }
}
