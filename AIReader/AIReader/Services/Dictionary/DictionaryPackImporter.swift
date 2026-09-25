import Foundation
import SQLiteData

/// Brings a dictionary into the app: packs this app wrote are copied as they
/// are, and the other formats are converted into one first.
enum DictionaryPackImporter {
    enum ImportError: LocalizedError {
        case nothingRecognized
        case unreadable
        case unsupportedSchema(String)

        var errorDescription: String? {
            switch self {
            case .nothingRecognized:
                "None of those files is a dictionary this app can read."
            case .unreadable:
                "That file is not a dictionary this app can read."
            case let .unsupportedSchema(version):
                "The dictionary uses schema version \(version); this app reads version 2."
            }
        }
    }

    /// Imports every dictionary among the picked files. StarDict spreads
    /// itself over several, so the files are grouped before anything is read.
    static func add(from urls: [URL]) throws -> [DictionaryPack.Draft] {
        let scoped = urls.filter { $0.startAccessingSecurityScopedResource() }
        defer { scoped.forEach { $0.stopAccessingSecurityScopedResource() } }

        let sources = DictionaryFormat.sources(from: urls)
        guard !sources.isEmpty else { throw ImportError.nothingRecognized }

        try FileManager.default.createDirectory(
            at: DictionaryStorage.root,
            withIntermediateDirectories: true
        )
        return try sources.map(add(source:))
    }

    private static func add(source: DictionarySource) throws -> DictionaryPack.Draft {
        let fileName = "\(UUID().uuidString).sqlite3"
        let destination = DictionaryStorage.root.appending(path: fileName)

        do {
            let info: DictionaryImportInfo
            if source.format == .native {
                try FileManager.default.copyItem(at: source.main, to: destination)
                info = try nativeInfo(of: destination, named: source.defaultName)
            } else {
                info = try DictionaryConverter.convert(source, to: destination)
            }
            return DictionaryPack.Draft(
                name: info.name,
                fileName: fileName,
                targetLanguage: info.targetLanguage,
                definitionLanguage: info.definitionLanguage
            )
        } catch {
            try? FileManager.default.removeItem(at: destination)
            throw error
        }
    }

    /// A pack this app wrote is taken as it is, once its schema checks out.
    private static func nativeInfo(of url: URL, named name: String) throws -> DictionaryImportInfo {
        let metadata = try metadata(of: url)
        let version = metadata["schema_version"] ?? "?"
        guard version == "2" else { throw ImportError.unsupportedSchema(version) }
        return DictionaryImportInfo(
            name: name,
            targetLanguage: metadata["target_language"],
            definitionLanguage: metadata["definition_language"]
        )
    }

    private static func metadata(of url: URL) throws -> [String: String] {
        var configuration = Configuration()
        configuration.readonly = true
        guard let queue = try? DatabaseQueue(path: url.path, configuration: configuration),
              let rows = try? queue.read({ try DictionaryMetadata.all.fetchAll($0) })
        else { throw ImportError.unreadable }
        return Dictionary(rows.map { ($0.key, $0.value) }, uniquingKeysWith: { first, _ in first })
    }
}
