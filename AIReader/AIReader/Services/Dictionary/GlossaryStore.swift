import ComposableArchitecture
import Foundation
import SQLiteData

/// Where a glossary's definitions are filed: an added dictionary pack, written
/// to one batch at a time. Several batches finish at once, so writes take
/// turns — the methods never suspend, or two batches could each find no pack
/// and begin one apiece.
actor GlossaryStore {
    static let shared = GlossaryStore()

    func definedForms(name: String) -> Set<String> {
        guard let url = pack(named: name)?.url else { return [] }
        var configuration = Configuration()
        configuration.readonly = true
        guard let queue = try? DatabaseQueue(path: url.path, configuration: configuration) else { return [] }
        return (try? queue.read { db in Set(try DictionaryLemma.select { $0.word }.fetchAll(db)) }) ?? []
    }

    func add(_ definitions: [String: String], name: String) throws {
        guard !definitions.isEmpty else { return }
        let existing = pack(named: name)
        let fileName = existing?.fileName ?? "\(UUID().uuidString).sqlite3"
        let url = DictionaryStorage.root.appending(path: fileName)

        let writer: DictionaryPackWriter
        if existing != nil {
            writer = try DictionaryPackWriter(appendingTo: url)
        } else {
            try FileManager.default.createDirectory(at: DictionaryStorage.root, withIntermediateDirectories: true)
            writer = try DictionaryPackWriter(creating: url)
        }
        for (form, definition) in definitions {
            try writer.add(DictionaryImportEntry(headword: form, partOfSpeech: nil, senses: [definition]))
        }
        try writer.finish(DictionaryImportInfo(name: name))

        guard existing == nil else { return }
        @Dependency(\.defaultDatabase) var database
        try database.write { db in
            try DictionaryPack.insert { DictionaryPack.Draft(name: name, fileName: fileName) }.execute(db)
        }
    }

    /// The added dictionary of that name; the bundled one is never a glossary.
    private func pack(named name: String) -> DictionaryPack? {
        @Dependency(\.defaultDatabase) var database
        let packs = try? database.read { db in
            try DictionaryPack.where { $0.name.eq(name) }.fetchAll(db)
        }
        return packs?.first { !$0.isBundled }
    }
}
