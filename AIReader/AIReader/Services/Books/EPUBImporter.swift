import Foundation

/// Unpacks an `.epub` into its own folder and reads the metadata needed to show
/// it in the library.
enum EPUBImporter {
    enum ImportError: LocalizedError {
        case missingContainer
        case missingPackage

        var errorDescription: String? {
            switch self {
            case .missingContainer: "The EPUB has no META-INF/container.xml."
            case .missingPackage: "The EPUB has no package document."
            }
        }
    }

    struct Result: Equatable, Sendable {
        let folder: String
        let title: String
        let author: String?
        let language: String?
        /// Both paths are relative to the book's folder.
        let packagePath: String
        let coverPath: String?
    }

    static func unpack(epubAt url: URL) throws -> Result {
        let archive = try ZIPArchive(data: try Data(contentsOf: url, options: .mappedIfSafe))

        guard let containerEntry = archive.entries.keys.first(where: {
            $0.caseInsensitiveCompare("META-INF/container.xml") == .orderedSame
        }),
            let packagePath = EPUBPackage.packagePath(inContainer: try archive.contents(of: containerEntry))
        else { throw ImportError.missingContainer }

        guard archive.entries[packagePath] != nil else { throw ImportError.missingPackage }
        let package = EPUBPackage.parse(try archive.contents(of: packagePath))

        let folder = UUID().uuidString
        let directory = BookStorage.directory(named: folder)
        do {
            try write(archive, to: directory)
        } catch {
            try? FileManager.default.removeItem(at: directory)
            throw error
        }

        let packageDirectory = (packagePath as NSString).deletingLastPathComponent
        return Result(
            folder: folder,
            title: package.title.isEmpty ? url.deletingPathExtension().lastPathComponent : package.title,
            author: package.author,
            language: package.language,
            packagePath: packagePath,
            coverPath: package.coverItem.map { resolve($0.href, relativeTo: packageDirectory) }
        )
    }

    /// Resolves an href that is relative to the package document into a path
    /// relative to the book folder.
    static func resolve(_ href: String, relativeTo directory: String) -> String {
        let combined = directory.isEmpty ? href : "\(directory)/\(href)"
        var components: [String] = []
        for part in combined.split(separator: "/") {
            switch part {
            case ".": continue
            case "..": _ = components.popLast()
            default: components.append(String(part))
            }
        }
        return components.joined(separator: "/")
    }

    private static func write(_ archive: ZIPArchive, to directory: URL) throws {
        let fileManager = FileManager.default
        for path in archive.entries.keys where !path.hasSuffix("/") {
            let destination = directory.appending(path: path)
            try fileManager.createDirectory(
                at: destination.deletingLastPathComponent(),
                withIntermediateDirectories: true
            )
            try archive.contents(of: path).write(to: destination, options: .atomic)
        }
    }
}
