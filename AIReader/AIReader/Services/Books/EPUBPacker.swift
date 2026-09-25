import Foundation

/// Puts an unpacked book back into an `.epub`, for sending to another
/// device. The importer keeps only the unpacked folder, so this is the file.
enum EPUBPacker {
    static func pack(_ directory: URL) throws -> Data {
        var writer = ZIPWriter()
        let mimetype = directory.appending(path: "mimetype")
        // An EPUB opens with its mimetype, uncompressed, so a reader can tell
        // what it is from the first bytes.
        let type = (try? Data(contentsOf: mimetype)) ?? Data("application/epub+zip".utf8)
        writer.add("mimetype", type, compress: false)

        let base = directory.standardizedFileURL.path(percentEncoded: false)
        guard let files = FileManager.default.enumerator(at: directory, includingPropertiesForKeys: [.isRegularFileKey])
        else { throw CocoaError(.fileReadNoSuchFile) }
        for case let file as URL in files {
            guard (try? file.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true else { continue }
            let full = file.standardizedFileURL.path(percentEncoded: false)
            let path = String(full.dropFirst(base.hasSuffix("/") ? base.count : base.count + 1))
            guard path != "mimetype" else { continue }
            writer.add(path, try Data(contentsOf: file))
        }
        return writer.finished()
    }
}
