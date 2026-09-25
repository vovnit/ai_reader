#include "DictionaryFormat.hpp"

#include "../../Support/Files.hpp"
#include "../../Support/Text.hpp"

#include <map>

const char* dictionaryFormatLabel(DictionaryFormat format) {
    switch (format) {
    case DictionaryFormat::Native: return "AIReader pack";
    case DictionaryFormat::Delimited: return "tab- or comma-separated";
    case DictionaryFormat::Xdxf: return "XDXF";
    case DictionaryFormat::Dsl: return "Lingvo DSL";
    case DictionaryFormat::StarDict: return "StarDict";
    }
    return "";
}

std::optional<std::string> DictionarySource::companion(const std::string& suffix) const {
    for (const auto& path : companions) {
        std::string name = Text::lower(Files::baseName(path));
        if (Text::endsWith(name, ".dz")) name = name.substr(0, name.size() - 3);
        if (Text::endsWith(name, suffix)) return path;
    }
    return std::nullopt;
}

std::string DictionarySource::defaultName() const {
    std::string name = DictionaryFormats::stem(main);
    if (Text::endsWith(Text::lower(name), ".dsl")) name = name.substr(0, name.size() - 4);
    return name;
}

namespace DictionaryFormats {

/// The extension, seeing through a trailing `.dz` or `.gz`.
static std::string extension(const std::string& path) {
    std::string name = Text::lower(Files::baseName(path));
    for (const char* wrapper : {".dz", ".gz"}) {
        if (Text::endsWith(name, wrapper)) name = name.substr(0, name.size() - 3);
    }
    auto dot = name.rfind('.');
    return dot == std::string::npos ? "" : name.substr(dot + 1);
}

std::string stem(const std::string& path) {
    std::string name = Files::baseName(path);
    for (const char* wrapper : {".dz", ".gz"}) {
        if (Text::endsWith(Text::lower(name), wrapper)) name = name.substr(0, name.size() - 3);
    }
    auto dot = name.rfind('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

/// Reads the first bytes for formats whose extension is unhelpful.
static std::optional<DictionaryFormat> sniff(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return std::nullopt;
    char head[512];
    size_t length = std::fread(head, 1, sizeof head, file);
    std::fclose(file);
    std::string text(head, length);
    if (Text::startsWith(text, "SQLite format 3")) return DictionaryFormat::Native;
    std::string lowered = Text::lower(text);
    if (Text::contains(lowered, "<xdxf")) return DictionaryFormat::Xdxf;
    if (Text::startsWith(lowered, "#name")) return DictionaryFormat::Dsl;
    return std::nullopt;
}

std::optional<DictionaryFormat> of(const std::string& path) {
    std::string ext = extension(path);
    if (ext == "sqlite3" || ext == "sqlite" || ext == "db") return DictionaryFormat::Native;
    if (ext == "tsv" || ext == "csv" || ext == "txt") return DictionaryFormat::Delimited;
    if (ext == "xdxf") return DictionaryFormat::Xdxf;
    if (ext == "dsl") return DictionaryFormat::Dsl;
    if (ext == "ifo") return DictionaryFormat::StarDict;
    if (ext == "xml") return sniff(path) == DictionaryFormat::Xdxf ? std::optional(DictionaryFormat::Xdxf) : std::nullopt;
    if (ext == "idx" || ext == "dict" || ext == "syn") return std::nullopt;  // StarDict companions
    return sniff(path);
}

std::vector<DictionarySource> sources(const std::vector<std::string>& paths) {
    std::vector<DictionarySource> result;
    for (const auto& path : paths) {
        if (auto format = of(path)) result.push_back({*format, path, {}});
    }
    for (auto& source : result) {
        if (source.format != DictionaryFormat::StarDict) continue;
        std::string base = stem(source.main);
        for (const auto& path : paths) {
            if (path != source.main && stem(path) == base) source.companions.push_back(path);
        }
    }
    return result;
}

}  // namespace DictionaryFormats

namespace LanguageName {

std::string code(const std::string& raw) {
    std::string text = Text::lower(Text::trim(raw));
    if (text.empty()) return "";
    if (text.size() == 2) return text;
    static const std::map<std::string, std::string> known = {
        {"fre", "fr"}, {"fra", "fr"}, {"french", "fr"}, {"eng", "en"}, {"english", "en"},
        {"ger", "de"}, {"deu", "de"}, {"german", "de"}, {"spa", "es"}, {"spanish", "es"},
        {"ita", "it"}, {"italian", "it"}, {"por", "pt"}, {"portuguese", "pt"}, {"rus", "ru"},
        {"russian", "ru"}, {"dut", "nl"}, {"nld", "nl"}, {"dutch", "nl"}, {"pol", "pl"},
        {"polish", "pl"}, {"ukr", "uk"}, {"ukrainian", "uk"}, {"jpn", "ja"}, {"japanese", "ja"},
        {"chi", "zh"}, {"zho", "zh"}, {"chinese", "zh"}, {"tur", "tr"}, {"turkish", "tr"},
        {"swe", "sv"}, {"swedish", "sv"}, {"lat", "la"}, {"latin", "la"}, {"gre", "el"}, {"greek", "el"},
    };
    auto found = known.find(text);
    return found == known.end() ? "" : found->second;
}

}  // namespace LanguageName
