import ComposableArchitecture
import Foundation
import SQLiteData

/// Offline dictionary lookups.
@DependencyClient
struct DictionaryClient: Sendable {
    var lookup: @Sendable (_ word: String) async -> DictionaryLookup = { DictionaryLookup(query: $0) }
    /// The articles under exactly this headword, the entry an answer was
    /// drawn from; none for a lemma the dictionary does not have.
    var articles: @Sendable (_ lemma: String) async -> [DictionaryLookup.Article] = { _ in [] }
}

extension DictionaryClient: DependencyKey {
    static var liveValue: Self {
        Self(
            lookup: { word in
                await DictionaryDatabase.shared.lookup(word, in: enabledPacks())
            },
            articles: { lemma in
                await DictionaryDatabase.shared.articlesFor(lemma, in: enabledPacks())
            }
        )
    }

    private static func enabledPacks() async -> [DictionaryPack] {
        @Dependency(\.defaultDatabase) var database
        return (try? await database.read { db in
            try DictionaryPack
                .where { $0.isEnabled }
                .order { $0.addedAt }
                .fetchAll(db)
        }) ?? []
    }
    static let testValue = Self()
}

extension DependencyValues {
    var dictionaryClient: DictionaryClient {
        get { self[DictionaryClient.self] }
        set { self[DictionaryClient.self] = newValue }
    }
}
