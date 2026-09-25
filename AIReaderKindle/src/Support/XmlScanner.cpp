#include "XmlScanner.hpp"

#include "Text.hpp"

#include <cstdlib>
#include <optional>

namespace XmlScanner {

namespace {

// The HTML 4 entities in code-point order from U+00A0, so a name is found by
// its position; the ones after that are the typographic set books use.
const char* const latin1Names[] = {
    "nbsp", "iexcl", "cent", "pound", "curren", "yen", "brvbar", "sect", "uml", "copy", "ordf", "laquo",
    "not", "shy", "reg", "macr", "deg", "plusmn", "sup2", "sup3", "acute", "micro", "para", "middot",
    "cedil", "sup1", "ordm", "raquo", "frac14", "frac12", "frac34", "iquest", "Agrave", "Aacute", "Acirc",
    "Atilde", "Auml", "Aring", "AElig", "Ccedil", "Egrave", "Eacute", "Ecirc", "Euml", "Igrave", "Iacute",
    "Icirc", "Iuml", "ETH", "Ntilde", "Ograve", "Oacute", "Ocirc", "Otilde", "Ouml", "times", "Oslash",
    "Ugrave", "Uacute", "Ucirc", "Uuml", "Yacute", "THORN", "szlig", "agrave", "aacute", "acirc", "atilde",
    "auml", "aring", "aelig", "ccedil", "egrave", "eacute", "ecirc", "euml", "igrave", "iacute", "icirc",
    "iuml", "eth", "ntilde", "ograve", "oacute", "ocirc", "otilde", "ouml", "divide", "oslash", "ugrave",
    "uacute", "ucirc", "uuml", "yacute", "thorn", "yuml",
};
const std::map<std::string, unsigned> otherEntities = {
    {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''},
    {"OElig", 0x152}, {"oelig", 0x153}, {"Scaron", 0x160}, {"scaron", 0x161}, {"Yuml", 0x178},
    {"fnof", 0x192}, {"circ", 0x2C6}, {"tilde", 0x2DC}, {"ensp", ' '}, {"emsp", ' '}, {"thinsp", ' '},
    {"zwnj", 0}, {"zwj", 0}, {"lrm", 0}, {"rlm", 0}, {"ndash", 0x2013}, {"mdash", 0x2014},
    {"lsquo", 0x2018}, {"rsquo", 0x2019}, {"sbquo", 0x201A}, {"ldquo", 0x201C}, {"rdquo", 0x201D},
    {"bdquo", 0x201E}, {"dagger", 0x2020}, {"Dagger", 0x2021}, {"bull", 0x2022}, {"hellip", 0x2026},
    {"permil", 0x2030}, {"prime", 0x2032}, {"Prime", 0x2033}, {"lsaquo", 0x2039}, {"rsaquo", 0x203A},
    {"oline", 0x203E}, {"frasl", 0x2044}, {"euro", 0x20AC}, {"trade", 0x2122}, {"larr", 0x2190},
    {"uarr", 0x2191}, {"rarr", 0x2192}, {"darr", 0x2193}, {"harr", 0x2194}, {"minus", 0x2212},
    {"infin", 0x221E}, {"ne", 0x2260}, {"le", 0x2264}, {"ge", 0x2265}, {"loz", 0x25CA},
};

/// The code point an entity name stands for: nothing for a name that is not
/// one, 0 for one that is invisible and best dropped, like the soft hyphen.
std::optional<unsigned> namedEntity(const std::string& name) {
    auto other = otherEntities.find(name);
    if (other != otherEntities.end()) return other->second;
    const size_t count = sizeof(latin1Names) / sizeof(latin1Names[0]);
    for (size_t i = 0; i < count; ++i) {
        if (name == latin1Names[i]) return name == "shy" ? 0 : 0xA0 + static_cast<unsigned>(i);
    }
    return std::nullopt;
}

void appendCodePoint(unsigned code, std::string& out) {
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

std::string localName(const std::string& name) {
    auto colon = name.rfind(':');
    return Text::lower(colon == std::string::npos ? name : name.substr(colon + 1));
}

bool isNameChar(char c) {
    return !(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '/' || c == '>' || c == '=');
}

/// Reads the `name="value"` pairs of a tag body.
Attributes attributes(const std::string& body) {
    Attributes result;
    size_t i = 0;
    while (i < body.size()) {
        while (i < body.size() && !isNameChar(body[i])) ++i;
        size_t nameStart = i;
        while (i < body.size() && isNameChar(body[i])) ++i;
        if (i == nameStart) break;
        std::string name = localName(body.substr(nameStart, i - nameStart));

        while (i < body.size() && (body[i] == ' ' || body[i] == '\t' || body[i] == '\r' || body[i] == '\n')) ++i;
        if (i >= body.size() || body[i] != '=') {
            result[name] = "";
            continue;
        }
        ++i;
        while (i < body.size() && (body[i] == ' ' || body[i] == '\t' || body[i] == '\r' || body[i] == '\n')) ++i;
        if (i >= body.size()) break;

        std::string value;
        if (body[i] == '"' || body[i] == '\'') {
            char quote = body[i++];
            size_t end = body.find(quote, i);
            if (end == std::string::npos) end = body.size();
            value = body.substr(i, end - i);
            i = end + 1;
        } else {
            size_t start = i;
            while (i < body.size() && isNameChar(body[i])) ++i;
            value = body.substr(start, i - start);
        }
        result[name] = decodeEntities(value);
    }
    return result;
}

}  // namespace

std::string decodeEntities(const std::string& text) {
    if (text.find('&') == std::string::npos) return text;
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '&') {
            out += text[i++];
            continue;
        }
        size_t end = text.find(';', i);
        if (end == std::string::npos || end - i > 10) {
            out += text[i++];
            continue;
        }
        std::string entity = text.substr(i + 1, end - i - 1);
        if (!entity.empty() && entity[0] == '#') {
            bool hex = entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X');
            unsigned code = static_cast<unsigned>(std::strtoul(entity.c_str() + (hex ? 2 : 1), nullptr, hex ? 16 : 10));
            if (code) appendCodePoint(code, out);
        } else {
            auto code = namedEntity(entity);
            if (!code) {
                out += text[i++];
                continue;
            }
            if (*code) appendCodePoint(*code, out);
        }
        i = end + 1;
    }
    return out;
}

void scan(const std::string& markup, const Handlers& handlers) {
    size_t i = 0;
    const size_t n = markup.size();

    auto emitText = [&](size_t start, size_t end, bool decode) {
        if (end <= start || !handlers.onText) return;
        std::string text = markup.substr(start, end - start);
        handlers.onText(decode ? decodeEntities(text) : text);
    };

    while (i < n) {
        size_t open = markup.find('<', i);
        if (open == std::string::npos) {
            emitText(i, n, true);
            break;
        }
        emitText(i, open, true);
        i = open;

        if (markup.compare(i, 4, "<!--") == 0) {
            size_t end = markup.find("-->", i + 4);
            i = end == std::string::npos ? n : end + 3;
            continue;
        }
        if (markup.compare(i, 9, "<![CDATA[") == 0) {
            size_t end = markup.find("]]>", i + 9);
            emitText(i + 9, end == std::string::npos ? n : end, false);
            i = end == std::string::npos ? n : end + 3;
            continue;
        }
        if (markup.compare(i, 2, "<?") == 0 || markup.compare(i, 2, "<!") == 0) {
            size_t end = markup.find('>', i);
            i = end == std::string::npos ? n : end + 1;
            continue;
        }

        // A tag. Quoted attribute values may contain '>', so skip over them.
        size_t end = i + 1;
        char quote = 0;
        while (end < n) {
            char c = markup[end];
            if (quote) {
                if (c == quote) quote = 0;
            } else if (c == '"' || c == '\'') {
                quote = c;
            } else if (c == '>') {
                break;
            }
            ++end;
        }
        std::string body = markup.substr(i + 1, end - i - 1);
        i = end < n ? end + 1 : n;
        if (body.empty()) continue;

        bool closing = body[0] == '/';
        bool selfClosing = body.back() == '/';
        if (closing) body.erase(0, 1);
        if (selfClosing) body.pop_back();

        size_t nameEnd = 0;
        while (nameEnd < body.size() && isNameChar(body[nameEnd])) ++nameEnd;
        std::string name = localName(body.substr(0, nameEnd));
        if (name.empty()) continue;

        if (closing) {
            if (handlers.onEnd) handlers.onEnd(name);
            continue;
        }
        if (handlers.onStart) handlers.onStart(name, attributes(body.substr(nameEnd)));
        if (selfClosing && handlers.onEnd) handlers.onEnd(name);
    }
}

}  // namespace XmlScanner
