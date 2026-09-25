import Foundation
import SQLiteData

/// A dictionary the app can search. One is bundled with the app; others are
/// added by the reader.
@Table
struct DictionaryPack: Identifiable, Equatable, Sendable {
    let id: Int
    var name = ""
    /// File name under `DictionaryStorage.root`, or nil for the bundled pack.
    var fileName: String?
    var targetLanguage: String?
    var definitionLanguage: String?
    var isEnabled = true
    var addedAt = Date()
}

extension DictionaryPack {
    var isBundled: Bool { fileName == nil }

    var url: URL? {
        guard let fileName else {
            return AppGroup.appBundle.url(forResource: "dictionary", withExtension: "sqlite3")
        }
        return DictionaryStorage.root.appending(path: fileName)
    }

    /// "French → Russian" when the pack says what it holds.
    var languages: String? {
        guard let targetLanguage, let definitionLanguage else { return nil }
        let name = { (code: String) in
            Locale.current.localizedString(forLanguageCode: code) ?? code
        }
        return "\(name(targetLanguage)) → \(name(definitionLanguage))"
    }
}

/// Where added dictionaries are kept: in the shared container, so the
/// extension can search them too.
enum DictionaryStorage {
    static var root: URL {
        AppGroup.container.appending(path: "Dictionaries", directoryHint: .isDirectory)
    }
}
