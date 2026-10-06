#include "PdfFilters.hpp"

#include "Inflate.hpp"

#include <cstdint>
#include <vector>

namespace PdfFilters {

namespace {

std::string flate(const std::string& data) {
    if (auto inflated = Inflate::zlib(data)) return *inflated;
    // A stream with a bad checksum, or none, still holds its text.
    if (data.size() > 2) {
        if (auto inflated = Inflate::raw(data.substr(2))) return *inflated;
    }
    return "";
}

std::string asciiHex(const std::string& data) {
    std::string out;
    int high = -1;
    for (char c : data) {
        if (c == '>') break;
        int value = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (value < 0) continue;
        if (high < 0) {
            high = value;
        } else {
            out += static_cast<char>(high * 16 + value);
            high = -1;
        }
    }
    if (high >= 0) out += static_cast<char>(high * 16);
    return out;
}

std::string ascii85(const std::string& data) {
    std::string out;
    uint32_t group = 0;
    int count = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        char c = data[i];
        if (c == '~') break;
        if (c == 'z' && count == 0) {
            out.append(4, '\0');
            continue;
        }
        if (c < '!' || c > 'u') continue;
        group = group * 85 + static_cast<uint32_t>(c - '!');
        if (++count == 5) {
            for (int shift = 24; shift >= 0; shift -= 8) out += static_cast<char>((group >> shift) & 0xFF);
            group = 0;
            count = 0;
        }
    }
    // A short last group stands for as many bytes as it has digits, less one.
    if (count > 1) {
        for (int pad = count; pad < 5; ++pad) group = group * 85 + 84;
        for (int i = 0; i < count - 1; ++i) out += static_cast<char>((group >> (24 - 8 * i)) & 0xFF);
    }
    return out;
}

std::string lzw(const std::string& data) {
    std::vector<std::string> table;
    auto reset = [&] {
        table.clear();
        for (int i = 0; i < 256; ++i) table.emplace_back(1, static_cast<char>(i));
        table.emplace_back();  // 256: clear
        table.emplace_back();  // 257: end
    };
    reset();
    std::string out;
    std::string previous;
    int width = 9;
    uint32_t buffer = 0;
    int bits = 0;
    for (unsigned char byte : data) {
        buffer = (buffer << 8) | byte;
        bits += 8;
        while (bits >= width) {
            int code = static_cast<int>((buffer >> (bits - width)) & ((1u << width) - 1));
            bits -= width;
            if (code == 256) {
                reset();
                width = 9;
                previous.clear();
                continue;
            }
            if (code == 257) return out;
            std::string entry;
            if (code < static_cast<int>(table.size())) entry = table[code];
            else if (!previous.empty()) entry = previous + previous[0];
            else return out;
            out += entry;
            if (!previous.empty()) table.push_back(previous + entry[0]);
            previous = entry;
            // The code width grows one entry early, as PDF's encoders do.
            int size = static_cast<int>(table.size()) + 1;
            if (size >= (1 << width) && width < 12) ++width;
        }
    }
    return out;
}

}  // namespace

std::optional<std::string> decode(const std::string& filter, const std::string& data) {
    if (filter == "FlateDecode" || filter == "Fl") return flate(data);
    if (filter == "ASCIIHexDecode" || filter == "AHx") return asciiHex(data);
    if (filter == "ASCII85Decode" || filter == "A85") return ascii85(data);
    if (filter == "LZWDecode" || filter == "LZW") return lzw(data);
    return std::nullopt;
}

}  // namespace PdfFilters
