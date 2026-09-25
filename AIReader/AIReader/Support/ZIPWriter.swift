import Foundation

/// Writes a ZIP archive, the mirror of `ZIPArchive`: entries stored or
/// deflated, no encryption, no ZIP64. Enough to put an EPUB back together.
struct ZIPWriter {
    private var body = Data()
    private var directory = Data()
    private var count = 0

    /// Adds a file. Entries are written in the order they are added, which
    /// matters for an EPUB: its `mimetype` must come first, uncompressed.
    mutating func add(_ path: String, _ contents: Data, compress: Bool = true) {
        let name = Data(path.utf8)
        // Deflating what is already compressed, a JPEG say, can make it
        // larger; it is stored as it is then.
        let deflated = compress ? Deflate.raw(contents).flatMap { $0.count < contents.count ? $0 : nil } : nil
        let stored = deflated ?? contents
        let method: UInt16 = deflated == nil ? 0 : 8
        let crc = Self.crc32(contents)
        let offset = UInt32(body.count)

        var local = Data()
        local.append32(0x0403_4B50)
        local.append16(20)          // version needed
        local.append16(0x0800)      // names are UTF-8
        local.append16(method)
        local.append16(0)           // time
        local.append16(0x21)        // date: 1 January 1980
        local.append32(crc)
        local.append32(UInt32(stored.count))
        local.append32(UInt32(contents.count))
        local.append16(UInt16(name.count))
        local.append16(0)           // extra field
        body.append(local)
        body.append(name)
        body.append(stored)

        directory.append32(0x0201_4B50)
        directory.append16(20)      // version made by
        directory.append(local[4..<26])
        directory.append16(UInt16(name.count))
        directory.append16(0)       // extra field
        directory.append16(0)       // comment
        directory.append16(0)       // disk
        directory.append16(0)       // internal attributes
        directory.append32(0)       // external attributes
        directory.append32(offset)
        directory.append(name)
        count += 1
    }

    /// The whole archive.
    func finished() -> Data {
        var end = Data()
        end.append32(0x0605_4B50)
        end.append16(0)
        end.append16(0)
        end.append16(UInt16(count))
        end.append16(UInt16(count))
        end.append32(UInt32(directory.count))
        end.append32(UInt32(body.count))
        end.append16(0)
        return body + directory + end
    }

    private static let table: [UInt32] = (0..<256).map { index in
        var value = UInt32(index)
        for _ in 0..<8 { value = value & 1 == 1 ? 0xEDB8_8320 ^ (value >> 1) : value >> 1 }
        return value
    }

    private static func crc32(_ data: Data) -> UInt32 {
        var crc: UInt32 = 0xFFFF_FFFF
        for byte in data { crc = table[Int((crc ^ UInt32(byte)) & 0xFF)] ^ (crc >> 8) }
        return crc ^ 0xFFFF_FFFF
    }
}

private extension Data {
    mutating func append16(_ value: UInt16) {
        Swift.withUnsafeBytes(of: value.littleEndian) { append(contentsOf: $0) }
    }

    mutating func append32(_ value: UInt32) {
        Swift.withUnsafeBytes(of: value.littleEndian) { append(contentsOf: $0) }
    }
}
