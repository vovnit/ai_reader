import Foundation

/// The text of a book file in whichever encoding it came in. A byte-order
/// mark settles it; UTF-16 without one shows itself by its zero bytes; then
/// UTF-8 wins whenever most of its multi-byte sequences are valid, whatever
/// the file declares, since declarations are often wrong and a stray bad
/// byte should cost one character, not the book. Otherwise FB2 and XHTML
/// name a single-byte encoding in the XML prolog and HTML in a meta tag, and
/// a file that names none is guessed: Western, windows-1251 or, since lib.ru
/// serves it undeclared, KOI8-R. Bad input is replaced as a browser's
/// TextDecoder replaces it, and the tables are the browsers' own, so a file
/// reads here as it does in the web app (`Support/Charset.js`) and on the
/// Kindle (`Support/Charset.cpp`).
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
    /// Code points for bytes 0x80–0xFF; lib.ru still serves Russian in KOI8-R.
    private static let koi8r: [UInt32] = [
        0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 0x2524,
        0x252C, 0x2534, 0x253C, 0x2580, 0x2584, 0x2588, 0x258C, 0x2590,
        0x2591, 0x2592, 0x2593, 0x2320, 0x25A0, 0x2219, 0x221A, 0x2248,
        0x2264, 0x2265, 0x00A0, 0x2321, 0x00B0, 0x00B2, 0x00B7, 0x00F7,
        0x2550, 0x2551, 0x2552, 0x0451, 0x2553, 0x2554, 0x2555, 0x2556,
        0x2557, 0x2558, 0x2559, 0x255A, 0x255B, 0x255C, 0x255D, 0x255E,
        0x255F, 0x2560, 0x2561, 0x0401, 0x2562, 0x2563, 0x2564, 0x2565,
        0x2566, 0x2567, 0x2568, 0x2569, 0x256A, 0x256B, 0x256C, 0x00A9,
        0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
        0x0445, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
        0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
        0x044C, 0x044B, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x044A,
        0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
        0x0425, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
        0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
        0x042C, 0x042B, 0x0417, 0x0428, 0x042D, 0x0429, 0x0427, 0x042A,
    ]
    /// Code points for bytes 0x80–0x9F; from 0xA0 the byte is the code point.
    private static let cp1252: [UInt32] = [
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    ]

    private static let cyrillicNames: Set = ["windows-1251", "cp1251", "x-cp1251"]
    private static let koi8Names: Set = ["koi8-r", "koi8-u"]
    private static let westernNames: Set = ["windows-1252", "cp1252", "iso-8859-1", "latin1", "us-ascii", "ascii"]

    /// The encoding a file names in its first bytes, `<?xml encoding="…"?>` or
    /// `<meta charset="…">`, lowercased; empty when none.
    static func declared(in data: Data) -> String {
        // ASCII only, lowercased, so every app reads the same name.
        let ascii = data.prefix(1024).map { $0 >= 0x80 ? 0x3F : (0x41...0x5A).contains($0) ? $0 + 0x20 : $0 }
        let head = String(decoding: ascii, as: UTF8.self) as NSString
        for pattern in [#"<\?xml[^>]*encoding[ \t\r\n]*=[ \t\r\n]*["']([^"']+)"#, #"<meta[^>]*charset[ \t\r\n]*=[ \t\r\n]*["']?([a-z0-9_-]+)"#] {
            guard let expression = try? NSRegularExpression(pattern: pattern),
                  let match = expression.firstMatch(in: head as String, range: NSRange(location: 0, length: head.length))
            else { continue }
            let isSpace: (Unicode.Scalar) -> Bool = { $0 == " " || $0 == "\t" || $0 == "\r" || $0 == "\n" }
            var name = Array(head.substring(with: match.range(at: 1)).unicodeScalars.drop(while: isSpace))
            while let last = name.last, isSpace(last) { name.removeLast() }
            return String(String.UnicodeScalarView(name))
        }
        return ""
    }

    /// `data` as text; `declared` is the encoding the file names, if any.
    static func decode(_ data: Data, declared: String = "") -> String {
        let bytes = [UInt8](data)
        if bytes.starts(with: [0xEF, 0xBB, 0xBF]) { return String(utf8(bytes[3...]).text) }
        if bytes.starts(with: [0xFF, 0xFE]) { return String(utf16(bytes, from: 2, bigEndian: false)) }
        if bytes.starts(with: [0xFE, 0xFF]) { return String(utf16(bytes, from: 2, bigEndian: true)) }
        if let bigEndian = unmarkedUTF16(bytes) { return String(utf16(bytes, from: 0, bigEndian: bigEndian)) }
        let unicode = utf8(bytes[...])
        if unicode.valid > unicode.invalid { return String(unicode.text) }
        let windows1251 = { (byte: UInt8) in cp1251[Int(byte) - 0x80] }
        let windows1252 = { (byte: UInt8) in byte < 0xA0 ? cp1252[Int(byte) - 0x80] : UInt32(byte) }
        if cyrillicNames.contains(declared) { return singleByte(bytes, windows1251) }
        let koi8 = { (byte: UInt8) in koi8r[Int(byte) - 0x80] }
        if koi8Names.contains(declared) { return singleByte(bytes, koi8) }
        if westernNames.contains(declared) { return singleByte(bytes, windows1252) }
        if !looksCyrillic(bytes) { return singleByte(bytes, windows1252) }
        return singleByte(bytes, looksKOI8(bytes) ? koi8 : windows1251)
    }

    /// Adds a decoded character as the EPUB can carry it: a byte-order mark
    /// doubled at the start dropped, a form feed or vertical tab a line break
    /// (a form-feed line is a page break, so it ends a paragraph), and the
    /// other controls and the non-characters U+FFFE and U+FFFF, which XML
    /// forbids, dropped.
    private static func add(_ code: UInt32, to text: inout String.UnicodeScalarView) {
        if code == 0xFEFF, text.isEmpty { return }
        if code == 0x0B || code == 0x0C {
            text.append("\n")
        } else if code > 0x08, code < 0x0E || code > 0x1F, code != 0x7F, code != 0xFFFE, code != 0xFFFF,
                  let scalar = Unicode.Scalar(code) {
            text.append(scalar)
        }
    }

    /// UTF-8 as a browser's TextDecoder reads it, and how many multi-byte
    /// sequences were valid and how many replaced.
    private static func utf8(_ bytes: ArraySlice<UInt8>) -> (text: String.UnicodeScalarView, valid: Int, invalid: Int) {
        var text = String.UnicodeScalarView()
        var valid = 0, invalid = 0
        var code: UInt32 = 0, needed = 0, seen = 0
        var lower: UInt8 = 0x80, upper: UInt8 = 0xBF
        var i = bytes.startIndex
        while i < bytes.endIndex {
            let b = bytes[i]
            i += 1
            if needed == 0 {
                switch b {
                case 0..<0x80:
                    add(UInt32(b), to: &text)
                case 0xC2...0xDF:
                    needed = 1
                    code = UInt32(b & 0x1F)
                case 0xE0...0xEF:
                    // The bounds rule out overlong forms and surrogates.
                    if b == 0xE0 { lower = 0xA0 }
                    if b == 0xED { upper = 0x9F }
                    needed = 2
                    code = UInt32(b & 0x0F)
                case 0xF0...0xF4:
                    if b == 0xF0 { lower = 0x90 }
                    if b == 0xF4 { upper = 0x8F }
                    needed = 3
                    code = UInt32(b & 0x07)
                default:
                    add(0xFFFD, to: &text)
                    invalid += 1
                }
                continue
            }
            if b < lower || b > upper {
                // The sequence so far is one replacement, and this byte starts afresh.
                add(0xFFFD, to: &text)
                invalid += 1
                needed = 0
                seen = 0
                lower = 0x80
                upper = 0xBF
                i -= 1
                continue
            }
            lower = 0x80
            upper = 0xBF
            code = code << 6 | UInt32(b & 0x3F)
            seen += 1
            if seen == needed {
                add(code, to: &text)
                valid += 1
                needed = 0
                seen = 0
            }
        }
        if needed > 0 {
            add(0xFFFD, to: &text)
            invalid += 1
        }
        return (text, valid, invalid)
    }

    /// UTF-16 from `start`, a lone surrogate or an odd last byte replaced as
    /// a browser's TextDecoder does.
    private static func utf16(_ bytes: [UInt8], from start: Int, bigEndian: Bool) -> String.UnicodeScalarView {
        var text = String.UnicodeScalarView()
        var high: UInt32 = 0
        var i = start
        while i + 1 < bytes.count {
            let a = UInt32(bytes[i]), b = UInt32(bytes[i + 1])
            let unit = bigEndian ? a << 8 | b : b << 8 | a
            i += 2
            if high != 0 {
                if (0xDC00...0xDFFF).contains(unit) {
                    add(0x10000 + ((high - 0xD800) << 10) + (unit - 0xDC00), to: &text)
                    high = 0
                    continue
                }
                add(0xFFFD, to: &text)
                high = 0
            }
            if (0xD800...0xDBFF).contains(unit) {
                high = unit
            } else {
                add((0xDC00...0xDFFF).contains(unit) ? 0xFFFD : unit, to: &text)
            }
        }
        // A pair cut short by the end, an odd last byte, or both: one replacement.
        if high != 0 || i < bytes.count { add(0xFFFD, to: &text) }
        return text
    }

    private static func singleByte(_ bytes: [UInt8], _ high: (UInt8) -> UInt32) -> String {
        var text = String.UnicodeScalarView()
        for byte in bytes {
            add(byte < 0x80 ? UInt32(byte) : high(byte), to: &text)
        }
        return String(text)
    }

    /// UTF-16 without a mark: its zero bytes fall at odd offsets
    /// (little-endian) or even ones, in at least one pair in sixteen of the
    /// first 1024 bytes, and hardly ever at the others, so a stray NUL in a
    /// UTF-8 file does not count, nor a run of them padding one out. Whether
    /// it is big-endian; nil when it is not UTF-16.
    private static func unmarkedUTF16(_ bytes: [UInt8]) -> Bool? {
        let pairs = min(bytes.count, 1024) / 2
        var even = 0, odd = 0
        for i in 0..<(pairs * 2) where bytes[i] == 0 {
            if i % 2 == 1 { odd += 1 } else { even += 1 }
        }
        let zeros = max(even, odd)
        if zeros == 0 || zeros * 16 < pairs || min(even, odd) * 4 >= zeros { return nil }
        return odd <= even
    }

    /// Cyrillic words in windows-1251 are runs of bytes from 0xC0, where А–я
    /// are; a Western accent sits inside a Latin word. Markup and base64 have
    /// no high bytes, so they do not sway it.
    private static func looksCyrillic(_ bytes: [UInt8]) -> Bool {
        let isLetter = { (byte: UInt8) in (0x41...0x5A).contains(byte) || (0x61...0x7A).contains(byte) }
        var runs = 0, mixed = 0
        for (a, b) in zip(bytes, bytes.dropFirst()) {
            if a >= 0xC0, b >= 0xC0 {
                runs += 1
            } else if (a >= 0xC0 && isLetter(b)) || (b >= 0xC0 && isLetter(a)) {
                mixed += 1
            }
        }
        return runs > mixed
    }

    /// Most letters are lower case, and KOI8-R puts а–я at 0xC0–0xDF where
    /// windows-1251 has А–Я, its own а–я being at 0xE0–0xFF.
    private static func looksKOI8(_ bytes: [UInt8]) -> Bool {
        let upper = bytes.count { $0 >= 0xE0 }
        let lower = bytes.count { $0 >= 0xC0 && $0 < 0xE0 }
        return lower > upper
    }
}
