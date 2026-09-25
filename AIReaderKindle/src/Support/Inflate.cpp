#include "Inflate.hpp"

#include <zlib.h>

namespace Inflate {

static std::optional<std::string> decode(const std::string& data, int windowBits) {
    z_stream stream{};
    if (inflateInit2(&stream, windowBits) != Z_OK) return std::nullopt;

    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    stream.avail_in = static_cast<uInt>(data.size());

    std::string out;
    char buffer[64 * 1024];
    int status = Z_OK;
    while (status != Z_STREAM_END) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer);
        stream.avail_out = sizeof buffer;
        status = inflate(&stream, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) {
            inflateEnd(&stream);
            return std::nullopt;
        }
        out.append(buffer, sizeof buffer - stream.avail_out);
        if (status == Z_OK && stream.avail_in == 0 && stream.avail_out != 0) break;
    }
    inflateEnd(&stream);
    return out;
}

std::optional<std::string> raw(const std::string& data) { return decode(data, -MAX_WBITS); }
std::optional<std::string> zlib(const std::string& data) { return decode(data, MAX_WBITS); }

bool isGzip(const std::string& data) {
    return data.size() > 18 && static_cast<unsigned char>(data[0]) == 0x1F
        && static_cast<unsigned char>(data[1]) == 0x8B && data[2] == 0x08;
}

std::optional<std::string> gzip(const std::string& data) {
    // zlib reads the gzip header itself, dictzip's extra field included; a
    // dictzip file is several members, which the loop below concatenates.
    if (!isGzip(data)) return std::nullopt;
    z_stream stream{};
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) return std::nullopt;
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    stream.avail_in = static_cast<uInt>(data.size());
    std::string out;
    char buffer[64 * 1024];
    while (true) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer);
        stream.avail_out = sizeof buffer;
        int status = inflate(&stream, Z_NO_FLUSH);
        out.append(buffer, sizeof buffer - stream.avail_out);
        if (status == Z_STREAM_END) {
            if (stream.avail_in == 0 || inflateReset(&stream) != Z_OK) break;
        } else if (status != Z_OK) {
            inflateEnd(&stream);
            return std::nullopt;
        } else if (stream.avail_in == 0 && stream.avail_out != 0) {
            break;
        }
    }
    inflateEnd(&stream);
    return out;
}

}  // namespace Inflate
