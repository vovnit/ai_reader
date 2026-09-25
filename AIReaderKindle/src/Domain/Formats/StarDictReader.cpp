#include "StarDictReader.hpp"

#include "../../Support/Files.hpp"
#include "../../Support/Inflate.hpp"
#include "../../Support/Text.hpp"
#include "../../Support/TextFile.hpp"

#include <map>

namespace Markup {

std::string stripTags(const std::string& text) {
    std::string output;
    int depth = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '<') {
            // Tags that break the line keep breaking it, so senses stay apart.
            std::string tag = Text::lower(text.substr(i + 1, 4));
            if (Text::startsWith(tag, "br") || Text::startsWith(tag, "p>") || Text::startsWith(tag, "p ")
                || Text::startsWith(tag, "/p>") || Text::startsWith(tag, "div") || Text::startsWith(tag, "li")) {
                output += '\n';
            }
            ++depth;
        } else if (c == '>') {
            if (depth) --depth;
        } else if (depth == 0) {
            output += c;
        }
    }
    output = Text::replaceAll(output, "&nbsp;", " ");
    output = Text::replaceAll(output, "&lt;", "<");
    output = Text::replaceAll(output, "&gt;", ">");
    output = Text::replaceAll(output, "&quot;", "\"");
    return Text::replaceAll(output, "&amp;", "&");
}

}  // namespace Markup

namespace StarDictReader {

namespace {

/// The `key=value` lines of an `.ifo`.
std::map<std::string, std::string> settings(const std::string& path) {
    std::map<std::string, std::string> result;
    auto lines = TextFile::lines(path);
    if (!lines) return result;
    for (const auto& line : *lines) {
        auto equals = line.find('=');
        if (equals == std::string::npos) continue;
        result[Text::lower(Text::trim(line.substr(0, equals)))] = Text::trim(line.substr(equals + 1));
    }
    return result;
}

/// A big-endian integer, moving the cursor past it.
unsigned long long number(const std::string& data, size_t& cursor, int bytes) {
    unsigned long long value = 0;
    for (int i = 0; i < bytes; ++i) {
        if (cursor >= data.size()) return 0;
        value = value << 8 | static_cast<unsigned char>(data[cursor++]);
    }
    return value;
}

/// Reads one field, unwrapping the markup its type implies.
std::string text(const std::string& field, char type) {
    switch (type) {
    case 'h': case 'x': case 'g': return Markup::stripTags(field);
    case 'm': case 'l': case 't': case 'y': case 'k': case 'w': return field;
    default: return "";
    }
}

/// Splits an article into its senses.
///
/// With `sametypesequence` the whole block is one field of a known type;
/// without it, each field announces its own type first.
std::vector<std::string> senses(const std::string& article, const std::string& sameType) {
    std::vector<std::string> texts;
    if (sameType.size() == 1) {
        texts.push_back(text(article, sameType[0]));
    } else {
        size_t cursor = 0;
        while (cursor < article.size()) {
            char type = article[cursor++];
            if (type >= 'A' && type <= 'Z') {  // a length-prefixed, and so binary, field
                size_t size = static_cast<size_t>(number(article, cursor, 4));
                cursor = std::min(article.size(), cursor + size);
            } else {
                size_t end = article.find('\0', cursor);
                if (end == std::string::npos) end = article.size();
                texts.push_back(text(article.substr(cursor, end - cursor), type));
                cursor = end < article.size() ? end + 1 : end;
            }
        }
    }
    std::vector<std::string> result;
    for (const auto& block : texts) {
        for (const auto& line : Text::split(block, '\n')) {
            std::string trimmed = Text::trim(line);
            if (!trimmed.empty()) result.push_back(trimmed);
        }
    }
    return result;
}

}  // namespace

std::optional<DictionaryImportInfo> read(const DictionarySource& source, const DictionaryEntrySink& entry, std::string* error) {
    auto info = settings(source.main);
    auto indexPath = source.companion(".idx");
    auto bodyPath = source.companion(".dict");
    if (!indexPath || !bodyPath) {
        if (error) *error = "A StarDict dictionary needs its .idx and .dict files beside the .ifo.";
        return std::nullopt;
    }
    auto index = Files::read(*indexPath);
    auto body = Files::read(*bodyPath);
    if (!index || !body) {
        if (error) *error = Files::baseName(source.main) + " could not be read.";
        return std::nullopt;
    }
    if (Inflate::isGzip(*body)) {
        auto inflated = Inflate::gzip(*body);
        if (!inflated) {
            if (error) *error = Files::baseName(*bodyPath) + " could not be read.";
            return std::nullopt;
        }
        body = inflated;
    }

    bool wide = info["idxoffsetbits"] == "64";
    std::string sameType = info["sametypesequence"];
    size_t cursor = 0;
    while (cursor < index->size()) {
        size_t end = index->find('\0', cursor);
        if (end == std::string::npos) break;
        std::string word = index->substr(cursor, end - cursor);
        cursor = end + 1;
        size_t offset = static_cast<size_t>(number(*index, cursor, wide ? 8 : 4));
        size_t size = static_cast<size_t>(number(*index, cursor, 4));
        if (size == 0 || offset + size > body->size()) continue;
        std::vector<std::string> found = senses(body->substr(offset, size), sameType);
        if (!word.empty() && !found.empty()) entry(DictionaryImportEntry{word, "", found});
    }

    DictionaryImportInfo result;
    result.name = info.count("bookname") ? info["bookname"] : source.defaultName();
    result.targetLanguage = LanguageName::code(info.count("lang") ? info["lang"] : info["sourcelang"]);
    result.definitionLanguage = LanguageName::code(info["targetlang"]);
    return result;
}

}  // namespace StarDictReader
