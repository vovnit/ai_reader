import ComposableArchitecture
import Foundation
import SQLiteData

/// Where a lookup came from: the word, the sentence around it, and the book it
/// was read in.
struct LookupContext: Equatable, Sendable {
    var word: String
    var sentence: String
    var language: String?
    var bookID: Book.ID?
}

/// Remembers explanations so the same word in the same sentence is only paid
/// for once.
@DependencyClient
struct LookupCacheClient: Sendable {
    var cached: @Sendable (_ context: LookupContext) async -> WordExplanation? = { _ in nil }
    var save: @Sendable (_ context: LookupContext, _ explanation: WordExplanation) async -> Void
    /// Forgets a lookup, and remembers that it was forgotten, so a sync does
    /// not bring it back from another device.
    var remove: @Sendable (_ lookup: Lookup) async -> Void
}

extension LookupCacheClient: DependencyKey {
    static var liveValue: Self {
        Self(
            cached: { context in
                @Dependency(\.defaultDatabase) var database
                return try? await database.read { db in
                    try Lookup
                        .where { $0.word.eq(context.word).and($0.sentence.eq(context.sentence)) }
                        .fetchOne(db)?
                        .explanation
                }
            },
            save: { context, explanation in
                @Dependency(\.defaultDatabase) var database
                @Dependency(\.date.now) var now
                try? await database.write { db in
                    try Lookup
                        .delete()
                        .where { $0.word.eq(context.word).and($0.sentence.eq(context.sentence)) }
                        .execute(db)
                    try LookupTombstone
                        .delete()
                        .where { $0.word.eq(context.word).and($0.sentence.eq(context.sentence)) }
                        .execute(db)
                    try Lookup.insert {
                        Lookup.Draft(
                            word: context.word,
                            sentence: context.sentence,
                            lemma: explanation.lemma,
                            formNote: explanation.formNote,
                            meaning: explanation.meaning,
                            language: context.language,
                            bookID: context.bookID,
                            guessed: explanation.guessed,
                            confidence: explanation.confidence,
                            lookedUpAt: now
                        )
                    }
                    .execute(db)
                }
            },
            remove: { lookup in
                @Dependency(\.defaultDatabase) var database
                @Dependency(\.date.now) var now
                try? await database.write { db in
                    try Lookup.delete().where { $0.id.eq(lookup.id) }.execute(db)
                    try LookupTombstone
                        .insert {
                            LookupTombstone(word: lookup.word, sentence: lookup.sentence, deletedAt: now)
                        } onConflict: { ($0.word, $0.sentence) } doUpdate: { row, excluded in
                            row.deletedAt = excluded.deletedAt
                        }
                        .execute(db)
                }
            }
        )
    }

    static let testValue = Self()
}

extension DependencyValues {
    var lookupCacheClient: LookupCacheClient {
        get { self[LookupCacheClient.self] }
        set { self[LookupCacheClient.self] = newValue }
    }
}
