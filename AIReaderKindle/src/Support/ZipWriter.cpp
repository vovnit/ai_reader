#include "ZipWriter.hpp"

#include <zlib.h>

namespace {

void put16(std::string& out, unsigned value) {
    out += static_cast<char>(value & 0xFF);
    out += static_cast<char>((value >> 8) & 0xFF);
}

void put32(std::string& out, unsigned long value) {
    put16(out, value & 0xFFFF);
    put16(out, (value >> 16) & 0xFFFF);
}

/// Raw DEFLATE, as ZIP stores it; empty when it fails.
std::string deflated(const std::string& data) {
    z_stream stream{};
    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) return "";
    std::string out(deflateBound(&stream, data.size()), '\0');
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    stream.avail_in = static_cast<uInt>(data.size());
    stream.next_out = reinterpret_cast<Bytef*>(&out[0]);
    stream.avail_out = static_cast<uInt>(out.size());
    int status = deflate(&stream, Z_FINISH);
    out.resize(stream.total_out);
    deflateEnd(&stream);
    return status == Z_STREAM_END ? out : "";
}

// 1980-01-01, the earliest a ZIP can say, so the same book makes the same file.
constexpr unsigned dosTime = 0;
constexpr unsigned dosDate = (0 << 9) | (1 << 5) | 1;

}  // namespace

void ZipWriter::add(const std::string& path, const std::string& contents, bool compress) {
    Entry entry{path, crc32(0, reinterpret_cast<const Bytef*>(contents.data()), static_cast<uInt>(contents.size())),
                contents.size(), contents.size(), 0, data_.size()};
    std::string body = contents;
    if (compress && !contents.empty()) {
        std::string smaller = deflated(contents);
        if (!smaller.empty() && smaller.size() < contents.size()) {
            body = std::move(smaller);
            entry.method = 8;
            entry.stored = body.size();
        }
    }
    put32(data_, 0x04034b50);
    put16(data_, 20);
    put16(data_, 0x0800);  // names are UTF-8
    put16(data_, entry.method);
    put16(data_, dosTime);
    put16(data_, dosDate);
    put32(data_, entry.crc);
    put32(data_, entry.stored);
    put32(data_, entry.size);
    put16(data_, static_cast<unsigned>(path.size()));
    put16(data_, 0);
    data_ += path;
    data_ += body;
    entries_.push_back(entry);
}

std::string ZipWriter::finished() const {
    std::string out = data_;
    size_t start = out.size();
    for (const auto& entry : entries_) {
        put32(out, 0x02014b50);
        put16(out, 20);
        put16(out, 20);
        put16(out, 0x0800);
        put16(out, entry.method);
        put16(out, dosTime);
        put16(out, dosDate);
        put32(out, entry.crc);
        put32(out, entry.stored);
        put32(out, entry.size);
        put16(out, static_cast<unsigned>(entry.path.size()));
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, 0);
        put32(out, entry.offset);
        out += entry.path;
    }
    size_t size = out.size() - start;
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, static_cast<unsigned>(entries_.size()));
    put16(out, static_cast<unsigned>(entries_.size()));
    put32(out, size);
    put32(out, start);
    put16(out, 0);
    return out;
}
