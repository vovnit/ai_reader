#include "ZipArchive.hpp"

#include "Files.hpp"
#include "Inflate.hpp"
#include "Text.hpp"

std::optional<ZipArchive> ZipArchive::open(const std::string& path, std::string* error) {
    ZipArchive archive;
    auto data = Files::read(path);
    if (!data) {
        if (error) *error = "The file could not be read.";
        return std::nullopt;
    }
    archive.data_ = std::move(*data);
    if (!archive.readCentralDirectory()) {
        if (error) *error = "The file is not a ZIP archive.";
        return std::nullopt;
    }
    return archive;
}

std::vector<std::string> ZipArchive::paths() const {
    std::vector<std::string> result;
    for (const auto& entry : entries_) result.push_back(entry.first);
    return result;
}

std::optional<std::string> ZipArchive::find(const std::string& path) const {
    if (entries_.count(path)) return path;
    std::string wanted = Text::lower(path);
    for (const auto& entry : entries_) {
        if (Text::lower(entry.first) == wanted) return entry.first;
    }
    return std::nullopt;
}

/// Scans backwards for the end-of-central-directory record, which sits at the
/// very end of the file followed only by an optional comment.
bool ZipArchive::readCentralDirectory() {
    const size_t minimum = 22;
    if (data_.size() < minimum) return false;
    size_t earliest = data_.size() > minimum + 0xFFFF ? data_.size() - minimum - 0xFFFF : 0;
    size_t offset = data_.size() - minimum;
    while (true) {
        if (u32(offset) == 0x06054b50) break;
        if (offset == earliest) return false;
        --offset;
    }

    size_t count = u16(offset + 10);
    size_t position = u32(offset + 16);
    for (size_t i = 0; i < count; ++i) {
        if (position + 46 > data_.size() || u32(position) != 0x02014b50) break;
        size_t nameLength = u16(position + 28);
        size_t extraLength = u16(position + 30);
        size_t commentLength = u16(position + 32);
        if (position + 46 + nameLength > data_.size()) break;
        std::string path = data_.substr(position + 46, nameLength);
        entries_[path] = Entry{u16(position + 10), u32(position + 20), u32(position + 42)};
        position += 46 + nameLength + extraLength + commentLength;
    }
    return true;
}

std::optional<std::string> ZipArchive::contents(const std::string& path) const {
    auto found = entries_.find(path);
    if (found == entries_.end()) return std::nullopt;
    const Entry& entry = found->second;
    size_t header = entry.headerOffset;
    if (header + 30 > data_.size() || u32(header) != 0x04034b50) return std::nullopt;

    // The local header repeats the name and extra field lengths, which are
    // allowed to differ from the central directory's.
    size_t start = header + 30 + u16(header + 26) + u16(header + 28);
    size_t end = start + entry.compressedSize;
    if (end > data_.size()) return std::nullopt;
    std::string payload = data_.substr(start, entry.compressedSize);

    switch (entry.method) {
    case 0: return payload;
    case 8: return Inflate::raw(payload);
    default: return std::nullopt;
    }
}

uint16_t ZipArchive::u16(size_t offset) const {
    return static_cast<uint8_t>(data_[offset]) | static_cast<uint8_t>(data_[offset + 1]) << 8;
}

uint32_t ZipArchive::u32(size_t offset) const {
    return static_cast<uint32_t>(u16(offset)) | static_cast<uint32_t>(u16(offset + 2)) << 16;
}
