import Foundation
import SQLiteData

@Table("metadata")
struct DictionaryMetadata: Hashable, Sendable {
    @Column(primaryKey: true)
    let key: String
    let value: String
}

@Table("lemmas")
struct DictionaryLemma: Hashable, Identifiable, Sendable {
    let id: Int64
    let word: String
}

@Table("forms")
struct DictionaryForm: Hashable, Sendable {
    @Column("normalized_form")
    let normalizedForm: String
    let ordinal: Int64
    let form: String
    @Column("lemma_id")
    let lemmaID: DictionaryLemma.ID
    @Column("part_of_speech")
    let partOfSpeech: String
    let gender: String?
    let number: String?
    @Column("verb_info")
    let verbInfo: String
}

@Table("entries")
struct DictionaryEntry: Hashable, Sendable {
    @Column("lemma_id")
    let lemmaID: DictionaryLemma.ID
    let ordinal: Int64
    let payload: Data
}

@Table("entries")
struct DefinitionIndexEntry: Hashable, Sendable {
    let word: String
    let offset: Int64
}
