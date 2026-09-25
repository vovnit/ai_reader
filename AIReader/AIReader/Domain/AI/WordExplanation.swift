import Foundation
import SQLiteData

/// What the model produces for a looked-up word.
struct WordExplanation: Equatable, Sendable, Codable {
    /// The dictionary form of the word.
    var lemma: String
    /// How the form found in the text relates to the lemma.
    var formNote: String
    /// A short, plain explanation of what the word means here.
    var meaning: String
    /// True when no dictionary entry supported the answer.
    var guessed: Bool
    /// The model's own confidence, from 0 to 1.
    var confidence: Double

    enum CodingKeys: String, CodingKey {
        case lemma
        case formNote = "form_note"
        case meaning
        case guessed
        case confidence
    }
}

/// A past lookup. Doubles as the cache: repeating a lookup of the same word in
/// the same sentence reuses the stored answer.
@Table
struct Lookup: Identifiable, Equatable, Sendable {
    let id: Int
    var word = ""
    var sentence = ""
    var lemma = ""
    var formNote = ""
    var meaning = ""
    var language: String?
    var bookID: Book.ID?
    var guessed = false
    var confidence = 0.0
    var lookedUpAt = Date()
}

extension Lookup {
    var explanation: WordExplanation {
        WordExplanation(
            lemma: lemma,
            formNote: formNote,
            meaning: meaning,
            guessed: guessed,
            confidence: confidence
        )
    }
}

/// A lookup that was deleted, kept by name so the deletion reaches other
/// devices instead of the word coming back from them.
@Table("lookupTombstones")
struct LookupTombstone: Equatable, Sendable {
    var word = ""
    var sentence = ""
    var deletedAt = Date()
}
