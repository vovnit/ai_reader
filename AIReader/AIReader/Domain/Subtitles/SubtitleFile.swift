import Foundation

/// One line of subtitles and when it is on screen.
struct SubtitleCue: Equatable, Sendable {
    let start: TimeInterval
    let end: TimeInterval
    /// The cue's lines joined with spaces, markup stripped.
    let text: String
}

/// Reads SubRip (.srt) and WebVTT (.vtt). Both are blocks separated by blank
/// lines: an optional index or name, a timing line, then the text.
enum SubtitleFile {
    static func cues(in contents: String) -> [SubtitleCue] {
        let unified = contents.replacingOccurrences(of: "\r\n", with: "\n").replacingOccurrences(of: "\r", with: "\n")
        return unified
            .components(separatedBy: "\n\n")
            .compactMap(cue(in:))
            .sorted { $0.start < $1.start }
    }

    private static func cue(in block: String) -> SubtitleCue? {
        let lines = block.split(separator: "\n", omittingEmptySubsequences: true)
        guard let timing = lines.firstIndex(where: { $0.contains("-->") }) else { return nil }
        let times = lines[timing].components(separatedBy: "-->")
        guard times.count == 2,
              let start = seconds(in: times[0]),
              let end = seconds(in: times[1])
        else { return nil }
        let text = lines[(timing + 1)...].map(plain).joined(separator: " ")
            .trimmingCharacters(in: .whitespaces)
        guard !text.isEmpty else { return nil }
        return SubtitleCue(start: start, end: end, text: text)
    }

    /// `HH:MM:SS,mmm` in SubRip, `HH:MM:SS.mmm` or `MM:SS.mmm` in WebVTT; the
    /// end may be followed by positioning settings.
    private static func seconds(in field: String) -> TimeInterval? {
        guard let stamp = field.split(separator: " ", omittingEmptySubsequences: true).first else { return nil }
        let parts = stamp.replacingOccurrences(of: ",", with: ".").split(separator: ":")
        guard (2...3).contains(parts.count) else { return nil }
        let numbers = parts.compactMap { Double($0) }
        guard numbers.count == parts.count else { return nil }
        return numbers.reduce(0) { $0 * 60 + $1 }
    }

    /// Strips `<i>…</i>` style tags and `{\an8}` positioning codes.
    private static func plain(_ line: Substring) -> String {
        var text = String(line)
        for pattern in ["<[^>]*>", "\\{[^}]*\\}"] {
            text = text.replacingOccurrences(of: pattern, with: "", options: .regularExpression)
        }
        return text
            .replacingOccurrences(of: "&amp;", with: "&")
            .replacingOccurrences(of: "&lt;", with: "<")
            .replacingOccurrences(of: "&gt;", with: ">")
            .replacingOccurrences(of: "&nbsp;", with: " ")
            .trimmingCharacters(in: .whitespaces)
    }
}
