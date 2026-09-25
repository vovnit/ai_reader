import Foundation
import SQLiteData

// The bundled dictionary is a read-only file produced by the DictionaryTool
// pipeline: `forms` maps every inflected form to a lemma, and `entries` holds
// one compressed JSON article per lemma.

@Table("metadata")
struct DictionaryMetadata: Equatable, Sendable {
    let key: String
    let value: String
}

@Table("lemmas")
struct DictionaryLemma: Identifiable, Equatable, Sendable {
    let id: Int
    let word: String
}

@Table("forms")
struct DictionaryForm: Equatable, Sendable {
    @Column("normalized_form") let normalizedForm: String
    let ordinal: Int
    let form: String
    @Column("lemma_id") let lemmaID: Int
    @Column("part_of_speech") let partOfSpeech: String
    let gender: String?
    let number: String?
    @Column("verb_info") let verbInfo: String
}

@Table("entries")
struct DictionaryEntryRow: Equatable, Sendable {
    @Column("lemma_id") let lemmaID: Int
    let ordinal: Int
    let payload: [UInt8]
}

/// One article as stored in an entry payload.
struct DictionaryArticlePayload: Decodable {
    struct Definition: Decodable {
        var glosses: [String]?
        var tags: [String]?
    }

    var partOfSpeech: String?
    var definitions: [Definition]?

    enum CodingKeys: String, CodingKey {
        case partOfSpeech = "part_of_speech"
        case definitions
    }
}
