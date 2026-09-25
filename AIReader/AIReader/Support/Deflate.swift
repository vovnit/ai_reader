import Compression
import Foundation

/// DEFLATE compression, the mirror of `Inflate`.
///
/// Converted dictionary packs store their articles the way the bundled one
/// does — a zlib stream per article — so they stay a quarter of the size.
enum Deflate {
    /// Wraps a raw DEFLATE stream in the zlib header and checksum.
    static func zlib(_ data: Data) -> Data? {
        guard let compressed = raw(data) else { return nil }
        var output = Data([0x78, 0x9C])
        output.append(compressed)
        withUnsafeBytes(of: adler32(data).bigEndian) { output.append(contentsOf: $0) }
        return output
    }

    /// A bare DEFLATE stream, with no header or checksum: what a ZIP entry
    /// holds.
    static func raw(_ data: Data) -> Data? {
        guard !data.isEmpty else { return Data() }

        var stream = compression_stream(
            dst_ptr: UnsafeMutablePointer<UInt8>(bitPattern: 1)!,
            dst_size: 0,
            src_ptr: UnsafePointer<UInt8>(bitPattern: 1)!,
            src_size: 0,
            state: nil
        )
        guard compression_stream_init(&stream, COMPRESSION_STREAM_ENCODE, COMPRESSION_ZLIB)
            == COMPRESSION_STATUS_OK
        else { return nil }
        defer { compression_stream_destroy(&stream) }

        let bufferSize = 64 * 1024
        let buffer = UnsafeMutablePointer<UInt8>.allocate(capacity: bufferSize)
        defer { buffer.deallocate() }

        var output = Data()
        let finished = data.withUnsafeBytes { source -> Bool in
            guard let base = source.bindMemory(to: UInt8.self).baseAddress else { return false }
            stream.src_ptr = base
            stream.src_size = source.count

            while true {
                stream.dst_ptr = buffer
                stream.dst_size = bufferSize
                let status = compression_stream_process(
                    &stream,
                    Int32(COMPRESSION_STREAM_FINALIZE.rawValue)
                )
                switch status {
                case COMPRESSION_STATUS_OK, COMPRESSION_STATUS_END:
                    output.append(buffer, count: bufferSize - stream.dst_size)
                    if status == COMPRESSION_STATUS_END { return true }
                default:
                    return false
                }
            }
        }
        return finished ? output : nil
    }

    /// The checksum zlib puts at the end of a stream.
    private static func adler32(_ data: Data) -> UInt32 {
        var low: UInt32 = 1
        var high: UInt32 = 0
        for byte in data {
            low = (low + UInt32(byte)) % 65521
            high = (high + low) % 65521
        }
        return high << 16 | low
    }
}
