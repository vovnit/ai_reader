#include "Json.hpp"

#include <cstdlib>
#include <cstring>

// The reading half of `Json`: a recursive-descent parser over the text.
namespace {

struct Parser {
    const std::string& text;
    size_t position = 0;

    void skipSpace() {
        while (position < text.size() && std::strchr(" \t\r\n", text[position])) ++position;
    }

    bool consume(const char* literal) {
        size_t length = std::strlen(literal);
        if (text.compare(position, length, literal) != 0) return false;
        position += length;
        return true;
    }

    static void appendCodePoint(unsigned code, std::string& out) {
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    bool hex4(unsigned& value) {
        if (position + 4 > text.size()) return false;
        value = 0;
        for (int i = 0; i < 4; ++i) {
            char c = text[position++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= c - '0';
            else if (c >= 'a' && c <= 'f') value |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') value |= c - 'A' + 10;
            else return false;
        }
        return true;
    }

    std::optional<std::string> string() {
        if (position >= text.size() || text[position] != '"') return std::nullopt;
        ++position;
        std::string out;
        while (position < text.size()) {
            char c = text[position++];
            if (c == '"') return out;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (position >= text.size()) return std::nullopt;
            char escape = text[position++];
            switch (escape) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned code;
                if (!hex4(code)) return std::nullopt;
                if (code >= 0xD800 && code <= 0xDBFF && consume("\\u")) {
                    unsigned low;
                    if (!hex4(low)) return std::nullopt;
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                appendCodePoint(code, out);
                break;
            }
            default: return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<Json> value() {
        skipSpace();
        if (position >= text.size()) return std::nullopt;
        char c = text[position];

        if (c == '"') {
            auto s = string();
            if (!s) return std::nullopt;
            return Json(*s);
        }
        if (c == '{') {
            ++position;
            Json object = Json::object();
            skipSpace();
            if (consume("}")) return object;
            while (true) {
                skipSpace();
                auto key = string();
                if (!key) return std::nullopt;
                skipSpace();
                if (!consume(":")) return std::nullopt;
                auto member = value();
                if (!member) return std::nullopt;
                object.set(*key, std::move(*member));
                skipSpace();
                if (consume(",")) continue;
                if (consume("}")) return object;
                return std::nullopt;
            }
        }
        if (c == '[') {
            ++position;
            Json array = Json::array();
            skipSpace();
            if (consume("]")) return array;
            while (true) {
                auto item = value();
                if (!item) return std::nullopt;
                array.push(std::move(*item));
                skipSpace();
                if (consume(",")) continue;
                if (consume("]")) return array;
                return std::nullopt;
            }
        }
        if (consume("true")) return Json(true);
        if (consume("false")) return Json(false);
        if (consume("null")) return Json(nullptr);

        const char* start = text.c_str() + position;
        char* end = nullptr;
        double number = std::strtod(start, &end);
        if (end == start) return std::nullopt;
        position += end - start;
        return Json(number);
    }
};

}  // namespace

std::optional<Json> Json::parse(const std::string& text) {
    Parser parser{text};
    auto result = parser.value();
    if (!result) return std::nullopt;
    parser.skipSpace();
    if (parser.position != text.size()) return std::nullopt;
    return result;
}
