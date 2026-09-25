import Foundation

/// A subtitle file as the viewer follows it: which cue is on screen at a
/// moment, and the sentence a cue belongs to, which often runs across several.
struct SubtitleTrack: Equatable, Sendable {
    /// A cue with the neighbours that complete its sentence, and where in
    /// that text the cue itself begins, as a UTF-16 offset.
    struct Passage: Equatable, Sendable {
        let text: String
        let cueOffset: Int
    }

    let cues: [SubtitleCue]
    let language: String?

    init(cues: [SubtitleCue]) {
        self.cues = cues
        language = TextLanguage.detect(in: cues.map(\.text).joined(separator: " "))
    }

    /// The latest cue that has started by `time`. A cue is kept after it
    /// leaves the screen, so a line can still be tapped once it has gone.
    func index(at time: TimeInterval) -> Int? {
        var low = 0
        var high = cues.count
        while low < high {
            let middle = (low + high) / 2
            if cues[middle].start <= time { low = middle + 1 } else { high = middle }
        }
        return low == 0 ? nil : low - 1
    }

    /// Walks back to the cue that opened the sentence and forward to the one
    /// that closes it. Stops at a long pause or after a few cues either way,
    /// since not every subtitle file punctuates.
    func passage(around index: Int) -> Passage {
        var first = index
        while first > 0, first > index - Self.reach, joins(first - 1, first), !endsSentence(first - 1) {
            first -= 1
        }
        var last = index
        while last < cues.count - 1, last < index + Self.reach, joins(last, last + 1), !endsSentence(last) {
            last += 1
        }
        let before = cues[first..<index].map(\.text).joined(separator: " ")
        let text = cues[first...last].map(\.text).joined(separator: " ")
        return Passage(text: text, cueOffset: before.isEmpty ? 0 : before.utf16.count + 1)
    }

    private static let reach = 3
    private static let longestPause: TimeInterval = 4

    private func joins(_ earlier: Int, _ later: Int) -> Bool {
        cues[later].start - cues[earlier].end < Self.longestPause
    }

    private func endsSentence(_ index: Int) -> Bool {
        let closing: Set<Character> = ["\"", "'", "»", "”", "’", ")", "]"]
        guard let mark = cues[index].text.last(where: { !closing.contains($0) && !$0.isWhitespace }) else {
            return false
        }
        return ".!?…".contains(mark)
    }
}
