#pragma once

#include <string>
#include <vector>

/// A ZIP archive written in memory: entries stored or deflated, no ZIP64.
/// Enough for an EPUB, whose `mimetype` must come first and uncompressed.
class ZipWriter {
public:
    void add(const std::string& path, const std::string& contents, bool compress = true);
    /// The archive, with its central directory.
    std::string finished() const;

private:
    struct Entry {
        std::string path;
        unsigned long crc;
        size_t size;
        size_t stored;
        int method;
        size_t offset;
    };

    std::string data_;
    std::vector<Entry> entries_;
};
