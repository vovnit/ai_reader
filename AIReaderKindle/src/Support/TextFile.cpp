#include "TextFile.hpp"

#include "Files.hpp"
#include "Inflate.hpp"
#include "Text.hpp"

#include <glib.h>

namespace TextFile {

namespace {

void appendCodePoint(gunichar code, std::string& out) {
    char buffer[8];
    int length = g_unichar_to_utf8(code, buffer);
    out.append(buffer, length);
}

std::string utf16(const std::string& data, bool bigEndian) {
    std::string out;
    gunichar high = 0;
    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        unsigned char a = data[i], b = data[i + 1];
        gunichar unit = bigEndian ? (a << 8 | b) : (b << 8 | a);
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            high = unit;
        } else if (unit >= 0xDC00 && unit <= 0xDFFF && high) {
            appendCodePoint(0x10000 + ((high - 0xD800) << 10) + (unit - 0xDC00), out);
            high = 0;
        } else {
            appendCodePoint(unit, out);
            high = 0;
        }
    }
    return out;
}

std::string latin1(const std::string& data) {
    std::string out;
    for (unsigned char c : data) appendCodePoint(c, out);
    return out;
}

}  // namespace

std::string decode(const std::string& data) {
    if (Text::startsWith(data, "\xFF\xFE")) return utf16(data.substr(2), false);
    if (Text::startsWith(data, "\xFE\xFF")) return utf16(data.substr(2), true);
    if (Text::startsWith(data, "\xEF\xBB\xBF")) return data.substr(3);
    // No mark: UTF-8 unless it does not decode, and then the two encodings
    // dictionary files otherwise turn up in.
    if (g_utf8_validate(data.c_str(), static_cast<gssize>(data.size()), nullptr)) return data;
    bool looksUtf16 = data.size() > 1 && data[1] == 0;
    return looksUtf16 ? utf16(data, false) : latin1(data);
}

std::optional<std::string> text(const std::string& path) {
    auto data = Files::read(path);
    if (!data) return std::nullopt;
    if (Inflate::isGzip(*data)) {
        auto inflated = Inflate::gzip(*data);
        if (!inflated) return std::nullopt;
        data = inflated;
    }
    return decode(*data);
}

std::optional<std::vector<std::string>> lines(const std::string& path) {
    auto contents = text(path);
    if (!contents) return std::nullopt;
    return Text::split(*contents, '\n');
}

}  // namespace TextFile
