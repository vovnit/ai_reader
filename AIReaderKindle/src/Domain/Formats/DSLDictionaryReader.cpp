#include "DSLDictionaryReader.hpp"

#include "../../Support/Text.hpp"
#include "../../Support/TextFile.hpp"

namespace DSLDictionaryReader {

/// `#NAME "Big Dictionary"` split into its parts.
static std::optional<std::pair<std::string, std::string>> directive(const std::string& line) {
    if (!Text::startsWith(line, "#")) return std::nullopt;
    auto space = line.find(' ');
    std::string key = Text::lower(space == std::string::npos ? line : line.substr(0, space));
    std::string value = space == std::string::npos ? "" : line.substr(space + 1);
    while (!value.empty() && (value.front() == ' ' || value.front() == '"')) value.erase(0, 1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '"' || value.back() == '\r')) value.pop_back();
    return std::make_pair(key, value);
}

std::string strip(const std::string& line) {
    std::string output;
    int depth = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        // A backslash escapes the character after it.
        if (c == '\\' && i + 1 < line.size()) {
            output += line[++i];
            continue;
        }
        // `{{...}}` are comments.
        if (c == '{' && i + 1 < line.size() && line[i + 1] == '{') {
            auto end = line.find("}}", i + 2);
            if (end != std::string::npos) {
                i = end + 1;
                continue;
            }
        }
        switch (c) {
        case '[': ++depth; break;
        case ']': if (depth) --depth; break;
        case '{': case '}': break;  // headword parts that are shown but not indexed
        case '\r': break;
        default: if (depth == 0) output += c;
        }
    }
    return output;
}

std::optional<DictionaryImportInfo> read(const DictionarySource& source, const DictionaryEntrySink& entry) {
    auto lines = TextFile::lines(source.main);
    if (!lines) return std::nullopt;

    DictionaryImportInfo info{source.defaultName(), "", ""};
    std::vector<std::string> headwords;
    std::vector<std::string> senses;
    auto flush = [&] {
        if (!headwords.empty() && !senses.empty()) {
            for (const auto& headword : headwords) entry(DictionaryImportEntry{headword, "", senses});
        }
        headwords.clear();
        senses.clear();
    };

    for (const auto& line : *lines) {
        if (auto found = directive(line)) {
            if (found->first == "#name") info.name = found->second;
            else if (found->first == "#index_language") info.targetLanguage = LanguageName::code(found->second);
            else if (found->first == "#contents_language") info.definitionLanguage = LanguageName::code(found->second);
            continue;
        }
        bool indented = !line.empty() && (line[0] == '\t' || line[0] == ' ');
        std::string text = Text::trim(strip(line));
        if (indented) {
            if (!text.empty()) senses.push_back(text);
        } else {
            // A run of headwords shares the article indented below it.
            if (!senses.empty()) flush();
            if (!text.empty()) headwords.push_back(text);
        }
    }
    flush();
    return info;
}

}  // namespace DSLDictionaryReader
