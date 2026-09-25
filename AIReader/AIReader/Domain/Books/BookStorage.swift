import Foundation

/// Where unpacked books live on disk. Books are addressed by folder name rather
/// than absolute path, because the app container path is not stable across
/// installs.
enum BookStorage {
    static var root: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        return base.appending(path: "Books", directoryHint: .isDirectory)
    }

    static func directory(named folder: String) -> URL {
        root.appending(path: folder, directoryHint: .isDirectory)
    }
}
