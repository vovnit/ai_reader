#include "DelimitedDictionaryReader.hpp"

#include "../../Support/Text.hpp"
#include "../../Support/TextFile.hpp"

namespace DelimitedDictionaryReader {

/// Splits a line, honouring the double quotes a spreadsheet export adds.
static std::vector<std::string> fields(const std::string& line, char separator) {
    std::vector<std::string> result;
    std::string field;
    bool quoted = false;
    for (char c : line) {
        if (c == '"') quoted = !quoted;
        else if (c == separator && !quoted) { result.push_back(Text::trim(field)); field.clear(); }
        else field += c;
    }
    result.push_back(Text::trim(field));
    std::vector<std::string> kept;
    for (auto& value : result) if (!value.empty()) kept.push_back(value);
    return kept;
}

void readText(const std::string& contents, const DictionaryEntrySink& entry) {
    std::vector<std::string> lines = Text::split(contents, '\n');

    // Tabs when the file uses any, commas otherwise.
    char separator = ',';
    for (size_t i = 0; i < lines.size() && i < 20; ++i) {
        if (lines[i].find('\t') != std::string::npos) { separator = '\t'; break; }
    }

    for (const auto& raw : lines) {
        std::string line = Text::trim(raw);
        if (line.empty() || line[0] == '#') continue;
        auto parts = fields(line, separator);
        if (parts.size() < 2) continue;
        entry(DictionaryImportEntry{parts[0], "", std::vector<std::string>(parts.begin() + 1, parts.end())});
    }
}

std::optional<DictionaryImportInfo> read(const DictionarySource& source, const DictionaryEntrySink& entry) {
    auto contents = TextFile::text(source.main);
    if (!contents) return std::nullopt;
    readText(*contents, entry);
    return DictionaryImportInfo{source.defaultName(), "", ""};
}

}  // namespace DelimitedDictionaryReader
