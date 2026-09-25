import Foundation
import SQLiteData

/// Read-only access to the dictionary packs: the one in the app bundle and
/// any the reader added.
actor DictionaryDatabase {
    static let shared = DictionaryDatabase()

    private var connections: [DictionaryPack.ID: any DatabaseReader] = [:]

    private func database(for pack: DictionaryPack) -> (any DatabaseReader)? {
        if let connection = connections[pack.id] { return connection }
        guard let url = pack.url else { return nil }
        var configuration = Configuration()
        configuration.readonly = true
        guard let queue = try? DatabaseQueue(path: url.path, configuration: configuration) else {
            return nil
        }
        connections[pack.id] = queue
        return queue
    }

    /// Searches every pack given and merges what they say about the word.
    func lookup(_ word: String, in packs: [DictionaryPack]) -> DictionaryLookup {
        let normalized = WordNormalizer.normalize(word)
        var result = DictionaryLookup(query: normalized)
        guard !normalized.isEmpty else { return result }

        for pack in packs {
            guard let database = database(for: pack) else { continue }
            search(normalized, in: database, named: pack.name, into: &result)
        }
        return result
    }

    /// The articles filed under exactly this headword: the entry a lemma
    /// came from. A form of another word brings nothing.
    func articlesFor(_ lemma: String, in packs: [DictionaryPack]) -> [DictionaryLookup.Article] {
        let normalized = WordNormalizer.normalize(lemma)
        guard !normalized.isEmpty else { return [] }
        var articles: [DictionaryLookup.Article] = []
        for pack in packs {
            guard let database = database(for: pack) else { continue }
            let found = try? database.read { db in
                try DictionaryLemma.where { $0.word.eq(normalized) }.fetchAll(db).flatMap { lemma in
                    try DictionaryEntryRow
                        .where { $0.lemmaID.eq(lemma.id) }
                        .order { $0.ordinal }
                        .fetchAll(db)
                        .compactMap { article(from: $0, lemma: lemma.word, source: pack.name) }
                }
            }
            articles += found ?? []
        }
        return articles
    }

    private func search(
        _ normalized: String,
        in database: any DatabaseReader,
        named source: String,
        into result: inout DictionaryLookup
    ) {
        do {
            try database.read { db in
                let forms = try DictionaryForm
                    .where { $0.normalizedForm.eq(normalized) }
                    .order { $0.ordinal }
                    .fetchAll(db)

                var lemmas = try forms.isEmpty
                    ? DictionaryLemma.where { $0.word.eq(normalized) }.fetchAll(db)
                    : lemmasByID(forms.map(\.lemmaID), db)

                // A form table hit whose lemma has no article is still worth
                // reporting; the direct headword may carry the definitions.
                if lemmas.isEmpty {
                    lemmas = try DictionaryLemma.where { $0.word.eq(normalized) }.fetchAll(db)
                }

                let names = Dictionary(lemmas.map { ($0.id, $0.word) }, uniquingKeysWith: { first, _ in first })
                result.forms += forms.compactMap { form in
                    names[form.lemmaID].map {
                        DictionaryLookup.Form(
                            lemma: $0,
                            partOfSpeech: form.partOfSpeech,
                            gender: form.gender,
                            number: form.number,
                            features: (try? JSONDecoder().decode([String].self, from: Data(form.verbInfo.utf8))) ?? []
                        )
                    }
                }

                for lemma in lemmas {
                    let rows = try DictionaryEntryRow
                        .where { $0.lemmaID.eq(lemma.id) }
                        .order { $0.ordinal }
                        .fetchAll(db)
                    result.articles += rows.compactMap {
                        article(from: $0, lemma: lemma.word, source: source)
                    }
                }
            }
        } catch {
            return
        }
    }

    private func lemmasByID(_ ids: [Int], _ db: Database) throws -> [DictionaryLemma] {
        var lemmas: [DictionaryLemma] = []
        for id in Set(ids) {
            lemmas += try DictionaryLemma.where { $0.id.eq(id) }.fetchAll(db)
        }
        return lemmas.sorted { $0.id < $1.id }
    }

    private func article(
        from row: DictionaryEntryRow,
        lemma: String,
        source: String
    ) -> DictionaryLookup.Article? {
        let stored = Data(row.payload)
        let json = Inflate.zlib(stored) ?? stored
        guard let payload = try? JSONDecoder().decode(DictionaryArticlePayload.self, from: json) else {
            return nil
        }
        let senses = (payload.definitions ?? []).flatMap { $0.glosses ?? [] }
        guard !senses.isEmpty else { return nil }
        return DictionaryLookup.Article(
            lemma: lemma,
            partOfSpeech: payload.partOfSpeech,
            senses: senses,
            source: source
        )
    }
}
