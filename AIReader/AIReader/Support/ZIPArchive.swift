import Foundation

/// Minimal read-only ZIP reader: enough of the format to unpack an EPUB.
///
/// Only the stored and deflated methods are supported, which is everything an
/// EPUB is allowed to use.
struct ZIPArchive {
    enum ReadError: LocalizedError {
        case notAZIPArchive
        case unsupportedCompression(UInt16)
        case corruptEntry(String)

        var errorDescription: String? {
            switch self {
            case .notAZIPArchive: "The file is not a ZIP archive."
            case let .unsupportedCompression(method): "Unsupported ZIP compression method \(method)."
            case let .corruptEntry(path): "The archive entry “\(path)” could not be read."
            }
        }
    }

    struct Entry {
        let path: String
        let compressionMethod: UInt16
        let compressedSize: Int
        let headerOffset: Int
    }

    private let data: Data
    private(set) var entries: [String: Entry] = [:]

    init(data: Data) throws {
        self.data = data
        guard let directory = Self.locateCentralDirectory(in: data) else {
            throw ReadError.notAZIPArchive
        }

        var offset = directory.offset
        for _ in 0..<directory.count {
            guard offset + 46 <= data.count, u32(data, offset) == 0x0201_4b50 else { break }
            let nameLength = Int(u16(data, offset + 28))
            let extraLength = Int(u16(data, offset + 30))
            let commentLength = Int(u16(data, offset + 32))
            let path = string(data, offset + 46, nameLength)
            entries[path] = Entry(
                path: path,
                compressionMethod: u16(data, offset + 10),
                compressedSize: Int(u32(data, offset + 20)),
                headerOffset: Int(u32(data, offset + 42))
            )
            offset += 46 + nameLength + extraLength + commentLength
        }
    }

    /// Decompresses one entry. Paths are the archive-internal paths.
    func contents(of path: String) throws -> Data {
        guard let entry = entries[path] else { throw ReadError.corruptEntry(path) }
        let header = entry.headerOffset
        guard header + 30 <= data.count, u32(data, header) == 0x0403_4b50 else {
            throw ReadError.corruptEntry(path)
        }

        // The local header repeats the name and extra field lengths, which are
        // allowed to differ from the central directory's.
        let start = header + 30 + Int(u16(data, header + 26)) + Int(u16(data, header + 28))
        let end = start + entry.compressedSize
        guard end <= data.count else { throw ReadError.corruptEntry(path) }
        let payload = data.subdata(in: (data.startIndex + start)..<(data.startIndex + end))

        switch entry.compressionMethod {
        case 0:
            return payload
        case 8:
            guard let inflated = Inflate.raw(payload) else { throw ReadError.corruptEntry(path) }
            return inflated
        default:
            throw ReadError.unsupportedCompression(entry.compressionMethod)
        }
    }

    // MARK: - Byte reading

    /// Scans backwards for the end-of-central-directory record, which sits at
    /// the very end of the file followed only by an optional comment.
    private static func locateCentralDirectory(in data: Data) -> (offset: Int, count: Int)? {
        let minimum = 22
        guard data.count >= minimum else { return nil }
        let earliest = max(0, data.count - minimum - 0xFFFF)
        var offset = data.count - minimum
        while offset >= earliest {
            if u32(data, offset) == 0x0605_4b50 {
                return (Int(u32(data, offset + 16)), Int(u16(data, offset + 10)))
            }
            offset -= 1
        }
        return nil
    }

    private static func u16(_ data: Data, _ offset: Int) -> UInt16 {
        let base = data.startIndex + offset
        return UInt16(data[base]) | UInt16(data[base + 1]) << 8
    }

    private static func u32(_ data: Data, _ offset: Int) -> UInt32 {
        UInt32(u16(data, offset)) | UInt32(u16(data, offset + 2)) << 16
    }

    private func u16(_ data: Data, _ offset: Int) -> UInt16 { Self.u16(data, offset) }
    private func u32(_ data: Data, _ offset: Int) -> UInt32 { Self.u32(data, offset) }

    private func string(_ data: Data, _ offset: Int, _ length: Int) -> String {
        let base = data.startIndex + offset
        let bytes = data.subdata(in: base..<(base + length))
        return String(data: bytes, encoding: .utf8) ?? ""
    }
}
