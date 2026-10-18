import Foundation

/// Walks XML or HTML and reports tags and text to closures as they come,
/// every run of text included, so a paragraph with emphasis inside reads
/// whole. Names come lowercased and without their namespace prefix; entities
/// in text and attributes are decoded. Tolerant of the broken markup books
/// carry, where `XMLScanner` (Foundation's parser) gives up at the first
/// error — so a book file reads here as it does in the Kindle app
/// (`Support/XmlScanner.cpp`) and the web app (`Support/XmlScanner.js`),
/// event for event.
enum MarkupScanner {
    struct Handlers {
        var onStart: (_ name: String, _ attributes: [String: String]) -> Void = { _, _ in }
        var onEnd: (_ name: String) -> Void = { _ in }
        var onText: (_ text: String) -> Void = { _ in }
    }

    static func scan(_ markup: String, _ handlers: Handlers) {
        let chars = Array(markup.unicodeScalars)
        let n = chars.count
        var i = 0

        func emitText(_ start: Int, _ end: Int, decode: Bool) {
            guard end > start else { return }
            let text = string(chars[start..<end])
            handlers.onText(decode ? decodeEntities(text) : text)
        }

        while i < n {
            var next = find("<", in: chars, from: i)
            while let at = next, !opensTag(chars, at + 1) { next = find("<", in: chars, from: at + 1) }
            guard let open = next else {
                emitText(i, n, decode: true)
                break
            }
            emitText(i, open, decode: true)
            i = open

            if starts(chars, i, "<!--") {
                let end = find("-->", in: chars, from: i + 4)
                i = end.map { $0 + 3 } ?? n
                continue
            }
            if starts(chars, i, "<![CDATA[") {
                let end = find("]]>", in: chars, from: i + 9)
                emitText(i + 9, end ?? n, decode: false)
                i = end.map { $0 + 3 } ?? n
                continue
            }
            if starts(chars, i, "<?") || starts(chars, i, "<!") {
                let end = find(">", in: chars, from: i)
                i = end.map { $0 + 1 } ?? n
                continue
            }

            // A tag. Quoted attribute values may contain '>', so skip over them.
            var end = i + 1
            var quote: Unicode.Scalar?
            while end < n {
                let c = chars[end]
                if let open = quote {
                    if c == open { quote = nil }
                } else if c == "\"" || c == "'" {
                    quote = c
                } else if c == ">" {
                    break
                }
                end += 1
            }
            var body = Array(chars[(i + 1)..<min(end, n)])
            i = end < n ? end + 1 : n
            guard !body.isEmpty else { continue }

            // The closing slash goes first, so "</>" leaves nothing to look at.
            let closing = body.first == "/"
            if closing { body.removeFirst() }
            let selfClosing = body.last == "/"
            if selfClosing { body.removeLast() }

            var nameEnd = 0
            while nameEnd < body.count, isNameChar(body[nameEnd]) { nameEnd += 1 }
            let name = localName(body[0..<nameEnd])
            guard !name.isEmpty else { continue }

            if closing {
                handlers.onEnd(name)
                continue
            }
            handlers.onStart(name, attributes(Array(body[nameEnd...])))
            if selfClosing { handlers.onEnd(name) }
        }
    }

    /// A reference is 1–9 ASCII letters, digits or '#' between '&' and ';'.
    private static let reference = try! NSRegularExpression(pattern: "&([A-Za-z0-9#]{1,9});")

    static func decodeEntities(_ text: String) -> String {
        guard text.unicodeScalars.contains("&") else { return text }
        let source = text as NSString
        var result = ""
        var last = 0
        for match in reference.matches(in: text, range: NSRange(location: 0, length: source.length)) {
            result += source.substring(with: NSRange(location: last, length: match.range.location - last))
            let entity = source.substring(with: match.range(at: 1))
            result += replacement(for: entity) ?? source.substring(with: match.range)
            last = match.range.location + match.range.length
        }
        return result + source.substring(from: last)
    }

    /// What `&entity;` stands for: the character of a known name, or of a
    /// number that is a character XML allows; "" for any other number; nil
    /// for an unknown name, which stays as written.
    private static func replacement(for entity: String) -> String? {
        let scalars = Array(entity.unicodeScalars)
        guard scalars.first == "#" else {
            guard let code = entities[entity] else { return nil }
            return code == 0 ? "" : Unicode.Scalar(code).map { String($0) } ?? ""
        }
        let hex = scalars.count > 1 && (scalars[1] == "x" || scalars[1] == "X")
        let digits = scalars.dropFirst(hex ? 2 : 1)
        let isDigit = { (c: Unicode.Scalar) in
            ("0"..."9").contains(c) || hex && (("a"..."f").contains(c) || ("A"..."F").contains(c))
        }
        guard !digits.isEmpty, digits.allSatisfy(isDigit),
              let code = UInt32(String(String.UnicodeScalarView(digits)), radix: hex ? 16 : 10),
              isXMLCharacter(code), let scalar = Unicode.Scalar(code)
        else { return "" }
        return String(scalar)
    }

    /// Tab, line breaks and the rest of Unicode but surrogates, U+FFFE and U+FFFF.
    private static func isXMLCharacter(_ code: UInt32) -> Bool {
        code == 0x09 || code == 0x0A || code == 0x0D || (0x20...0xD7FF).contains(code)
            || (0xE000...0xFFFD).contains(code) || (0x10000...0x10FFFF).contains(code)
    }

    // The HTML 4 entities in code-point order from U+00A0, so a name is found
    // by its position; the ones after that are the typographic set books use.
    private static let latin1Names = [
        "nbsp", "iexcl", "cent", "pound", "curren", "yen", "brvbar", "sect", "uml", "copy", "ordf", "laquo",
        "not", "shy", "reg", "macr", "deg", "plusmn", "sup2", "sup3", "acute", "micro", "para", "middot",
        "cedil", "sup1", "ordm", "raquo", "frac14", "frac12", "frac34", "iquest", "Agrave", "Aacute", "Acirc",
        "Atilde", "Auml", "Aring", "AElig", "Ccedil", "Egrave", "Eacute", "Ecirc", "Euml", "Igrave", "Iacute",
        "Icirc", "Iuml", "ETH", "Ntilde", "Ograve", "Oacute", "Ocirc", "Otilde", "Ouml", "times", "Oslash",
        "Ugrave", "Uacute", "Ucirc", "Uuml", "Yacute", "THORN", "szlig", "agrave", "aacute", "acirc", "atilde",
        "auml", "aring", "aelig", "ccedil", "egrave", "eacute", "ecirc", "euml", "igrave", "iacute", "icirc",
        "iuml", "eth", "ntilde", "ograve", "oacute", "ocirc", "otilde", "ouml", "divide", "oslash", "ugrave",
        "uacute", "ucirc", "uuml", "yacute", "thorn", "yuml",
    ]

    private static let entities: [String: UInt32] = {
        var entities: [String: UInt32] = [
            "amp": 0x26, "lt": 0x3C, "gt": 0x3E, "quot": 0x22, "apos": 0x27,
            "OElig": 0x152, "oelig": 0x153, "Scaron": 0x160, "scaron": 0x161, "Yuml": 0x178,
            "fnof": 0x192, "circ": 0x2C6, "tilde": 0x2DC, "ensp": 0x20, "emsp": 0x20, "thinsp": 0x20,
            "zwnj": 0, "zwj": 0, "lrm": 0, "rlm": 0, "ndash": 0x2013, "mdash": 0x2014,
            "lsquo": 0x2018, "rsquo": 0x2019, "sbquo": 0x201A, "ldquo": 0x201C, "rdquo": 0x201D,
            "bdquo": 0x201E, "dagger": 0x2020, "Dagger": 0x2021, "bull": 0x2022, "hellip": 0x2026,
            "permil": 0x2030, "prime": 0x2032, "Prime": 0x2033, "lsaquo": 0x2039, "rsaquo": 0x203A,
            "oline": 0x203E, "frasl": 0x2044, "euro": 0x20AC, "trade": 0x2122, "larr": 0x2190,
            "uarr": 0x2191, "rarr": 0x2192, "darr": 0x2193, "harr": 0x2194, "minus": 0x2212,
            "infin": 0x221E, "ne": 0x2260, "le": 0x2264, "ge": 0x2265, "loz": 0x25CA,
        ]
        // The soft hyphen is invisible and best dropped.
        for (index, name) in latin1Names.enumerated() {
            entities[name] = name == "shy" ? 0 : 0xA0 + UInt32(index)
        }
        return entities
    }()

    /// The `name="value"` pairs of a tag body.
    private static func attributes(_ body: [Unicode.Scalar]) -> [String: String] {
        var result: [String: String] = [:]
        var i = 0
        while i < body.count {
            while i < body.count, !isNameChar(body[i]) { i += 1 }
            let nameStart = i
            while i < body.count, isNameChar(body[i]) { i += 1 }
            if i == nameStart { break }
            let name = localName(body[nameStart..<i])
            while i < body.count, isWhite(body[i]) { i += 1 }
            guard i < body.count, body[i] == "=" else {
                result[name] = ""
                continue
            }
            i += 1
            while i < body.count, isWhite(body[i]) { i += 1 }
            if i >= body.count { break }
            let value: String
            if body[i] == "\"" || body[i] == "'" {
                let quote = body[i]
                i += 1
                let end = body[i...].firstIndex(of: quote) ?? body.count
                value = string(body[i..<end])
                i = end + 1
            } else {
                let start = i
                while i < body.count, isNameChar(body[i]) { i += 1 }
                value = string(body[start..<i])
            }
            result[name] = decodeEntities(value)
        }
        return result
    }

    /// Lowercased by ASCII alone, as markup names are, whatever the locale.
    private static func localName(_ name: ArraySlice<Unicode.Scalar>) -> String {
        let local = name.lastIndex(of: ":").map { name[($0 + 1)...] } ?? name
        return String(String.UnicodeScalarView(local.map { ("A"..."Z").contains($0) ? Unicode.Scalar($0.value + 0x20)! : $0 }))
    }

    /// A '<' opens a tag only before a letter, '/', '!' or '?'; any other is
    /// text, as a browser keeps "1 < 2".
    private static func opensTag(_ chars: [Unicode.Scalar], _ at: Int) -> Bool {
        guard at < chars.count else { return false }
        let c = chars[at]
        return ("a"..."z").contains(c) || ("A"..."Z").contains(c) || c == "/" || c == "!" || c == "?"
    }

    private static func isNameChar(_ c: Unicode.Scalar) -> Bool {
        !(c == " " || c == "\t" || c == "\r" || c == "\n" || c == "/" || c == ">" || c == "=")
    }

    private static func isWhite(_ c: Unicode.Scalar) -> Bool {
        c == " " || c == "\t" || c == "\r" || c == "\n"
    }

    private static func string(_ scalars: ArraySlice<Unicode.Scalar>) -> String {
        String(String.UnicodeScalarView(scalars))
    }

    private static func starts(_ chars: [Unicode.Scalar], _ at: Int, _ prefix: String) -> Bool {
        let scalars = Array(prefix.unicodeScalars)
        guard at + scalars.count <= chars.count else { return false }
        return Array(chars[at..<(at + scalars.count)]) == scalars
    }

    private static func find(_ needle: String, in chars: [Unicode.Scalar], from start: Int) -> Int? {
        let scalars = Array(needle.unicodeScalars)
        guard !scalars.isEmpty, start <= chars.count - scalars.count else { return nil }
        var i = start
        while i <= chars.count - scalars.count {
            if chars[i] == scalars[0], Array(chars[i..<(i + scalars.count)]) == scalars { return i }
            i += 1
        }
        return nil
    }
}
