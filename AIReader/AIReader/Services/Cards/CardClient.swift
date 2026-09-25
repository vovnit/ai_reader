import ComposableArchitecture
import Foundation
import SQLiteData

/// Every lookup as a flash card. The cards themselves are the lookups; only
/// how each has fared in practice is kept apart, and it goes when the lookup
/// does.
@DependencyClient
struct CardClient: Sendable {
    /// Every card, newest lookup first; `bookID` narrows it to one book.
    var all: @Sendable (_ bookID: Book.ID?) async -> [Card] = { _ in [] }
    /// Counts one practice outcome against the card.
    var record: @Sendable (_ lookupID: Lookup.ID, _ correct: Bool) async -> Void
}

extension CardClient: DependencyKey {
    static var liveValue: Self {
        Self(
            all: { bookID in
                @Dependency(\.defaultDatabase) var database
                return (try? await database.read { db in
                    let lookups = try Lookup
                        .where { bookID.map($0.bookID.eq) ?? true }
                        .order { $0.lookedUpAt.desc() }
                        .fetchAll(db)
                    let practice = Dictionary(
                        try CardPractice.all.fetchAll(db).map { ($0.lookupID, $0) },
                        uniquingKeysWith: { first, _ in first }
                    )
                    return lookups.map { Card(lookup: $0, practice: practice[$0.id]) }
                }) ?? []
            },
            record: { lookupID, correct in
                @Dependency(\.defaultDatabase) var database
                @Dependency(\.date.now) var now
                try? await database.write { db in
                    try CardPractice
                        .insert {
                            CardPractice(
                                lookupID: lookupID,
                                correct: correct ? 1 : 0,
                                wrong: correct ? 0 : 1,
                                practicedAt: now
                            )
                        } onConflict: { $0.lookupID } doUpdate: { row, excluded in
                            row.correct += excluded.correct
                            row.wrong += excluded.wrong
                            row.practicedAt = excluded.practicedAt
                        }
                        .execute(db)
                }
            }
        )
    }

    static let testValue = Self()
}

extension DependencyValues {
    var cardClient: CardClient {
        get { self[CardClient.self] }
        set { self[CardClient.self] = newValue }
    }
}
