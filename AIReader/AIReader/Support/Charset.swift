import Foundation

/// The text of a book file in whichever encoding it came in. FB2 and XHTML
/// declare theirs in the XML prolog and HTML in a meta tag; a .txt says
/// nothing, so a file that is not UTF-8 is read as windows-1251 when more of
/// its letters are Cyrillic than Latin, else as windows-1252. The tables are
/// the browsers' own, so a file reads here as it does in the web app
/// (`Support/Charset.js`) and on the Kindle (`Support/Charset.cpp`).
enum Charset {
    /// Code points for bytes 0x80–0xFF.
    private static let cp1251: [UInt32] = [
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
        0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
        0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x0098, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
        0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
        0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
        0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
        0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
        0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
        0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
        0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
        0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
        0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
        0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
        0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
        0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
    ]
    /// Code points for bytes 0x80–0x9F; from 0xA0 the byte is the code point.
    private static let cp1252: [UInt32] = [
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    ]

    private static let cyrillicNames: Set = ["windows-1251", "cp1251", "x-cp1251"]
    private static let westernNames: Set = ["windows-1252", "cp1252", "iso-8859-1", "latin1", "us-ascii", "ascii"]

    /// The encoding a file names in its first bytes, `<?xml encoding="…"?>` or
    /// `<meta charset="…">`, lowercased; empty when none.
    static func declared(in data: Data) -> String {
        let head = String(decoding: data.prefix(1024).map { $0 < 0x80 ? $0 : 0x3F }, as: UTF8.self)
        for pattern in [#"<\?xml[^>]*encoding\s*=\s*["']([^"']+)"#, #"<meta[^>]*charset\s*=\s*["']?([\w-]+)"#] {
            guard let expression = try? NSRegularExpression(pattern: pattern, options: .caseInsensitive),
                  let match = expression.firstMatch(in: head, range: NSRange(head.startIndex..., in: head)),
                  let range = Range(match.range(at: 1), in: head)
            else { continue }
            return head[range].trimmingCharacters(in: .whitespaces).lowercased()
        }
        return ""
    }

    /// `data` as text; `declared` is the encoding the file names, if any.
    static func decode(_ data: Data, declared: String = "") -> String {
        let bytes = [UInt8](data)
        let bom = bytes.starts(with: [0xEF, 0xBB, 0xBF]) || bytes.starts(with: [0xFF, 0xFE]) || bytes.starts(with: [0xFE, 0xFF])
        // UTF-16 without a mark shows itself by its zero bytes, which UTF-8 also allows.
        if bom || (bytes.count > 1 && (bytes[0] == 0 || bytes[1] == 0)) {
            return TextFile.decode(data)
        }
        if cyrillicNames.contains(declared) { return singleByte(bytes, cyrillic: true) }
        if westernNames.contains(declared) { return singleByte(bytes, cyrillic: false) }
        if let text = String(data: data, encoding: .utf8) { return text }
        return singleByte(bytes, cyrillic: looksCyrillic(bytes))
    }

    private static func singleByte(_ bytes: [UInt8], cyrillic: Bool) -> String {
        var scalars = String.UnicodeScalarView()
        for byte in bytes {
            let code: UInt32 = byte < 0x80 ? UInt32(byte)
                : cyrillic ? cp1251[Int(byte) - 0x80]
                : byte < 0xA0 ? cp1252[Int(byte) - 0x80] : UInt32(byte)
            scalars.append(Unicode.Scalar(code) ?? "\u{FFFD}")
        }
        return String(scalars)
    }

    /// More letter bytes in 0xC0–0xFF, where windows-1251 keeps А–я, than ASCII letters.
    private static func looksCyrillic(_ bytes: [UInt8]) -> Bool {
        var high = 0
        var ascii = 0
        for byte in bytes {
            if byte >= 0xC0 {
                high += 1
            } else if (0x41...0x5A).contains(byte) || (0x61...0x7A).contains(byte) {
                ascii += 1
            }
        }
        return high > ascii
    }
}
