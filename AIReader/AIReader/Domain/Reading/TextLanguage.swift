import Foundation
import NaturalLanguage

/// The language a text is written in, when the recognizer is sure enough.
enum TextLanguage {
    /// Samples from the middle of the text: front matter is often in another
    /// language. Nil for a text too short to judge.
    static func detect(in text: String) -> String? {
        let body = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard body.count > 200 else { return nil }
        let start = body.index(body.startIndex, offsetBy: body.count / 3)
        let end = body.index(start, offsetBy: min(4000, body.count - body.count / 3))

        let recognizer = NLLanguageRecognizer()
        recognizer.processString(String(body[start..<end]))
        guard let language = recognizer.dominantLanguage,
              recognizer.languageHypotheses(withMaximum: 1)[language] ?? 0 > 0.5
        else { return nil }
        return language.rawValue
    }
}
