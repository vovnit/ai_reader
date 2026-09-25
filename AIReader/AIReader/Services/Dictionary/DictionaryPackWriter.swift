import Foundation
import SQLiteData

/// Writes the schema version 2 pack the app reads, one article at a time.
///
/// Converted dictionaries carry no inflection tables, so `forms` is left empty
/// and lookups fall back to matching the headword. That is why headwords are
/// stored normalized: the reader taps “Paris” and the query arrives as “paris”.
final class DictionaryPackWriter {
    enum WriteError: LocalizedError {
        case empty

        var errorDescription: String? {
            switch self {
            case .empty: "The file held no entries this app could read."
            }
        }
    }

    private let queue: DatabaseQueue
    private var lemmaIDs: [String: Int] = [:]
    private var ordinals: [Int: Int] = [:]
    private var pending: [(lemma: Int, ordinal: Int, payload: Data)] = []
    private var newLemmas: [(id: Int, word: String)] = []

    private(set) var entryCount = 0

    init(creating url: URL) throws {
        try? FileManager.default.removeItem(at: url)
        queue = try DatabaseQueue(path: url.path)
        try queue.write { db in
            try db.execute(sql: "CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL)")
            try db.execute(sql: "CREATE TABLE lemmas (id INTEGER PRIMARY KEY, word TEXT NOT NULL UNIQUE)")
            try db.execute(sql: """
                CREATE TABLE forms (
                    normalized_form TEXT NOT NULL,
                    ordinal INTEGER NOT NULL,
                    form TEXT NOT NULL,
                    lemma_id INTEGER NOT NULL,
                    part_of_speech TEXT NOT NULL,
                    gender TEXT,
                    number TEXT,
                    verb_info TEXT NOT NULL,
                    PRIMARY KEY (normalized_form, ordinal)
                ) WITHOUT ROWID
                """)
            try db.execute(sql: """
                CREATE TABLE entries (
                    lemma_id INTEGER NOT NULL,
                    ordinal INTEGER NOT NULL,
                    payload BLOB NOT NULL,
                    PRIMARY KEY (lemma_id, ordinal)
                ) WITHOUT ROWID
                """)
        }
    }

    func add(_ entry: DictionaryImportEntry) throws {
        let word = WordNormalizer.normalize(entry.headword)
        let senses = entry.senses.map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
            .filter { !$0.isEmpty }
        guard !word.isEmpty, !senses.isEmpty else { return }

        let id = lemmaID(for: word)
        let ordinal = ordinals[id, default: 0]
        ordinals[id] = ordinal + 1

        guard let payload = payload(partOfSpeech: entry.partOfSpeech, senses: senses) else { return }
        pending.append((id, ordinal, payload))
        entryCount += 1
        if pending.count >= 2_000 { try flush() }
    }

    func finish(_ info: DictionaryImportInfo) throws {
        try flush()
        guard entryCount > 0 else { throw WriteError.empty }

        let metadata: [String: String] = [
            "schema_version": "2",
            "mode": "converted",
            "payload_encoding": "json+zlib",
            "lemma_count": String(lemmaIDs.count),
            "form_count": "0",
            "definition_entry_count": String(entryCount),
            "target_language": info.targetLanguage ?? "",
            "definition_language": info.definitionLanguage ?? ""
        ]
        try queue.write { db in
            for (key, value) in metadata where !value.isEmpty {
                try db.execute(sql: "INSERT INTO metadata (key, value) VALUES (?, ?)",
                               arguments: [key, value])
            }
        }
    }

    private func lemmaID(for word: String) -> Int {
        if let id = lemmaIDs[word] { return id }
        let id = lemmaIDs.count + 1
        lemmaIDs[word] = id
        newLemmas.append((id, word))
        return id
    }

    /// The article shape `DictionaryArticlePayload` decodes.
    private func payload(partOfSpeech: String?, senses: [String]) -> Data? {
        var article: [String: Any] = ["definitions": [["glosses": senses]]]
        if let partOfSpeech, !partOfSpeech.isEmpty { article["part_of_speech"] = partOfSpeech }
        guard let json = try? JSONSerialization.data(withJSONObject: article) else { return nil }
        return Deflate.zlib(json) ?? json
    }

    private func flush() throws {
        guard !pending.isEmpty || !newLemmas.isEmpty else { return }
        let lemmas = newLemmas
        let entries = pending
        newLemmas = []
        pending = []

        try queue.write { db in
            for lemma in lemmas {
                try db.execute(sql: "INSERT INTO lemmas (id, word) VALUES (?, ?)",
                               arguments: [lemma.id, lemma.word])
            }
            for entry in entries {
                try db.execute(
                    sql: "INSERT INTO entries (lemma_id, ordinal, payload) VALUES (?, ?, ?)",
                    arguments: [entry.lemma, entry.ordinal, entry.payload]
                )
            }
        }
    }
}
