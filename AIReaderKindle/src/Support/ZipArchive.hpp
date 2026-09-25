#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

/// Minimal read-only ZIP reader: enough of the format to read an EPUB. Only
/// the stored and deflated methods are supported, which is everything an EPUB
/// is allowed to use.
class ZipArchive {
public:
    static std::optional<ZipArchive> open(const std::string& path, std::string* error = nullptr);

    std::vector<std::string> paths() const;
    bool contains(const std::string& path) const { return entries_.count(path) > 0; }
    /// The archive-internal path whose case-insensitive spelling matches.
    std::optional<std::string> find(const std::string& path) const;
    /// The decompressed bytes of one entry, or nothing if it is missing or corrupt.
    std::optional<std::string> contents(const std::string& path) const;

private:
    struct Entry {
        uint16_t method;
        uint32_t compressedSize;
        uint32_t headerOffset;
    };

    std::string data_;
    std::map<std::string, Entry> entries_;

    bool readCentralDirectory();
    uint16_t u16(size_t offset) const;
    uint32_t u32(size_t offset) const;
};
