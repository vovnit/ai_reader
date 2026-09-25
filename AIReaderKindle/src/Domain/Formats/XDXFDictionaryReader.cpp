#include "XDXFDictionaryReader.hpp"

#include "../../Support/Text.hpp"
#include "../../Support/TextFile.hpp"
#include "../../Support/XmlScanner.hpp"

namespace XDXFDictionaryReader {

std::optional<DictionaryImportInfo> read(const DictionarySource& source, const DictionaryEntrySink& entry) {
    auto markup = TextFile::text(source.main);
    if (!markup) return std::nullopt;

    DictionaryImportInfo info{source.defaultName(), "", ""};
    std::vector<std::string> headwords;
    std::string body;
    std::string text;
    bool inArticle = false, inKey = false, inName = false;

    XmlScanner::Handlers handlers;
    handlers.onStart = [&](const std::string& name, const XmlScanner::Attributes& attributes) {
        auto attribute = [&](const char* key) {
            auto found = attributes.find(key);
            return found == attributes.end() ? std::string() : found->second;
        };
        if (name == "xdxf") {
            info.targetLanguage = LanguageName::code(attribute("lang_from"));
            info.definitionLanguage = LanguageName::code(attribute("lang_to"));
        } else if (name == "ar") {
            inArticle = true;
            headwords.clear();
            body.clear();
        } else if (name == "k" && inArticle) {
            inKey = true;
            text.clear();
        } else if (name == "full_name") {
            inName = true;
            text.clear();
        }
    };
    handlers.onText = [&](const std::string& chunk) {
        if (inKey || inName) text += chunk;
        else if (inArticle) body += chunk;
    };
    handlers.onEnd = [&](const std::string& name) {
        if (name == "full_name") {
            inName = false;
            std::string full = Text::trim(text);
            if (!full.empty()) info.name = full;
        } else if (name == "k") {
            inKey = false;
            std::string headword = Text::trim(text);
            if (!headword.empty()) headwords.push_back(headword);
        } else if (name == "ar") {
            inArticle = false;
            std::vector<std::string> senses;
            for (const auto& line : Text::split(body, '\n')) {
                std::string trimmed = Text::trim(line);
                if (!trimmed.empty()) senses.push_back(trimmed);
            }
            if (senses.empty()) return;
            for (const auto& headword : headwords) entry(DictionaryImportEntry{headword, "", senses});
        }
    };
    XmlScanner::scan(*markup, handlers);
    return info;
}

}  // namespace XDXFDictionaryReader
