import Foundation

/// Cards as the text file Anki imports: one note per line, fields a tab
/// apart, under the header lines newer Anki reads and older Anki skips.
/// The front is the word with its lemma and the sentence it was met in;
/// the back is the meaning it had there.
enum AnkiExport {
    static func text(cards: [Card], deck: String) -> String {
        var out = "#separator:tab\n#html:true\n#deck:\(line(deck))\n"
        for card in cards {
            var front = "<b>\(field(card.front))</b>"
            if let lemma = card.lemma { front += " (\(field(lemma)))" }
            if !card.sentence.isEmpty { front += "<br><i>\(field(card.sentence))</i>" }
            out += "\(front)\t\(field(card.back))\n"
        }
        return out
    }

    /// Fields are read as HTML, so what would pass for markup is escaped, and
    /// a field must stay on its line.
    private static func field(_ text: String) -> String {
        var out = ""
        for c in text {
            switch c {
            case "&": out += "&amp;"
            case "<": out += "&lt;"
            case ">": out += "&gt;"
            case "\t", "\n", "\r": out += " "
            default: out.append(c)
            }
        }
        return out
    }

    private static func line(_ text: String) -> String {
        String(text.map { $0 == "\t" || $0 == "\n" || $0 == "\r" ? " " : $0 })
    }
}
