import Compression
import Foundation

/// DEFLATE decompression on top of the system `Compression` framework.
///
/// ZIP entries store raw DEFLATE streams; the bundled dictionary stores zlib
/// streams, which are the same payload wrapped in a 2-byte header and a
/// trailing checksum.
enum Inflate {
    /// Inflates a raw DEFLATE stream.
    static func raw(_ data: Data) -> Data? {
        decode(data)
    }

    /// Inflates a zlib stream by skipping its header; the decoder stops on the
    /// end-of-stream marker, so the trailing checksum is simply ignored.
    static func zlib(_ data: Data) -> Data? {
        guard data.count > 2 else { return nil }
        return decode(data.dropFirst(2))
    }

    /// Inflates a gzip stream by stepping over its variable-length header.
    /// StarDict's `.dict.dz` and Lingvo's `.dsl.dz` are both plain gzip.
    static func gzip(_ data: Data) -> Data? {
        let bytes = [UInt8](data)
        guard bytes.count > 18, bytes[0] == 0x1F, bytes[1] == 0x8B, bytes[2] == 0x08 else {
            return nil
        }
        let flags = bytes[3]
        var index = 10

        if flags & 0x04 != 0 {  // FEXTRA, the field dictzip stores its index in
            guard index + 1 < bytes.count else { return nil }
            index += 2 + Int(bytes[index]) + Int(bytes[index + 1]) << 8
        }
        for flag in [UInt8(0x08), UInt8(0x10)] where flags & flag != 0 {  // FNAME, FCOMMENT
            while index < bytes.count, bytes[index] != 0 { index += 1 }
            index += 1
        }
        if flags & 0x02 != 0 { index += 2 }  // FHCRC

        guard index < bytes.count else { return nil }
        return decode(data.dropFirst(index))
    }

    private static func decode(_ data: Data) -> Data? {
        guard !data.isEmpty else { return Data() }

        var stream = compression_stream(
            dst_ptr: UnsafeMutablePointer<UInt8>(bitPattern: 1)!,
            dst_size: 0,
            src_ptr: UnsafePointer<UInt8>(bitPattern: 1)!,
            src_size: 0,
            state: nil
        )
        guard compression_stream_init(&stream, COMPRESSION_STREAM_DECODE, COMPRESSION_ZLIB)
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
}
