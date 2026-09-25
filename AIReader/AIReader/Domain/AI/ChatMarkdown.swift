import Foundation

/// A model's answer, which is usually Markdown, made readable as one styled
/// string. Inline markup is Foundation's; the block structure a plain text
/// view would lose — paragraphs, headings, lists, fences — is kept line by line.
enum ChatMarkdown {
    static func render(_ markdown: String) -> AttributedString {
        var result = AttributedString()
        var inFence = false
        for (index, line) in markdown.components(separatedBy: .newlines).enumerated() {
            if index > 0 { result.append(AttributedString("\n")) }
            let trimmed = line.trimmingCharacters(in: .whitespaces)
            if trimmed.hasPrefix("```") {
                inFence.toggle()
                continue
            }
            if inFence {
                var code = AttributedString(line)
                code.inlinePresentationIntent = .code
                result.append(code)
            } else {
                result.append(renderLine(line, trimmed: trimmed))
            }
        }
        return result
    }

    private static func renderLine(_ line: String, trimmed: String) -> AttributedString {
        if let heading = trimmed.firstMatch(of: #/^#{1,6}\s+(.*)$/#) {
            var text = inline(String(heading.1))
            text.inlinePresentationIntent = .stronglyEmphasized
            return text
        }
        if trimmed.firstMatch(of: #/^([-*_]\s*){3,}$/#) != nil {
            return AttributedString("———")
        }
        let indent = String(line.prefix { $0 == " " || $0 == "\t" })
        if let bullet = trimmed.firstMatch(of: #/^[-*+]\s+(.*)$/#) {
            return AttributedString(indent + "• ") + inline(String(bullet.1))
        }
        if let quote = trimmed.firstMatch(of: #/^>\s?(.*)$/#) {
            var text = inline(String(quote.1))
            text.inlinePresentationIntent = .emphasized
            return AttributedString("│ ") + text
        }
        // Numbered items keep their numbers as written.
        return AttributedString(indent) + inline(trimmed)
    }

    /// Bold, italics, code and links within one line; the line itself if it
    /// does not parse.
    private static func inline(_ text: String) -> AttributedString {
        let options = AttributedString.MarkdownParsingOptions(
            interpretedSyntax: .inlineOnlyPreservingWhitespace
        )
        return (try? AttributedString(markdown: text, options: options)) ?? AttributedString(text)
    }
}
