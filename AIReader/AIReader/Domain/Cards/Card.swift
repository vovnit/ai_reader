import Foundation
import SQLiteData

/// A flash card made from a lookup, the way Anki holds one: the word on the
/// front, what it meant there on the back, and the sentence it was met in
/// with the word blanked out. The practice record says how it has fared.
struct Card: Equatable, Sendable, Identifiable {
    let lookupID: Lookup.ID
    /// The form met in the text.
    var front: String
    /// Its dictionary form, when that differs from the front.
    var lemma: String?
    /// The meaning that fit the sentence.
    var back: String
    /// The sentence it was met in, and the same with the word blanked.
    var sentence: String
    var example: String
    var correct = 0
    var wrong = 0
    /// When it was last practised; nil until then.
    var practicedAt: Date?

    var id: Lookup.ID { lookupID }

    init(lookup: Lookup, practice: CardPractice? = nil) {
        lookupID = lookup.id
        front = lookup.word
        lemma = lookup.lemma == lookup.word || lookup.lemma.isEmpty ? nil : lookup.lemma
        back = lookup.meaning
        sentence = lookup.sentence
        example = Self.blank(lookup.sentence, word: lookup.word)
        correct = practice?.correct ?? 0
        wrong = practice?.wrong ?? 0
        practicedAt = practice?.practicedAt
    }

    /// `sentence` with every whole-word occurrence of `word` blanked.
    static func blank(_ sentence: String, word: String) -> String {
        guard !word.isEmpty else { return sentence }
        let gap = "____"
        var out = sentence
        var from = out.startIndex
        while let found = out.range(of: word, range: from..<out.endIndex) {
            // "a" inside "avait" is not the word.
            let letterBefore = found.lowerBound > out.startIndex
                && out[out.index(before: found.lowerBound)].isLetterOrNumber
            let letterAfter = found.upperBound < out.endIndex && out[found.upperBound].isLetterOrNumber
            if letterBefore || letterAfter {
                from = out.index(after: found.lowerBound)
                continue
            }
            out.replaceSubrange(found, with: gap)
            from = out.index(found.lowerBound, offsetBy: gap.count)
        }
        return out
    }

    /// Up to `count` cards to practise next: the never-practised first, then
    /// the most-missed, then the longest unseen. Ties keep the given order.
    static func due(_ cards: [Card], count: Int) -> [Card] {
        let sorted = cards.enumerated().sorted { a, b in
            precedes(a.element, b.element) ?? (a.offset < b.offset)
        }
        return sorted.prefix(count).map(\.element)
    }

    /// True when `a` is more due than `b`; nil when they are equally due.
    private static func precedes(_ a: Card, _ b: Card) -> Bool? {
        if (a.practicedAt == nil) != (b.practicedAt == nil) { return a.practicedAt == nil }
        let missedA = a.wrong - a.correct, missedB = b.wrong - b.correct
        if missedA != missedB { return missedA > missedB }
        if let at = a.practicedAt, let bt = b.practicedAt, at != bt { return at < bt }
        return nil
    }
}

/// How each flash card has fared in practice. The card is the lookup itself,
/// so the record goes when the lookup does.
@Table("cardPractice")
struct CardPractice: Equatable, Sendable {
    let lookupID: Lookup.ID
    var correct = 0
    var wrong = 0
    var practicedAt = Date()
}

private extension Character {
    var isLetterOrNumber: Bool { isLetter || isNumber }
}
