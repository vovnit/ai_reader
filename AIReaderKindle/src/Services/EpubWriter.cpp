#include "EpubWriter.hpp"

#include "../Domain/Books/EpubPackage.hpp"
#include "../Domain/Books/LanguageDetector.hpp"
#include "../Domain/Books/RemoteBookName.hpp"
#include "../Support/Files.hpp"
#include "../Support/Text.hpp"
#include "../Support/XmlScanner.hpp"
#include "../Support/ZipArchive.hpp"
#include "../Support/ZipWriter.hpp"

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <exception>

namespace EpubWriter {

namespace {

std::string now() {
    std::time_t seconds = std::time(nullptr);
    std::tm utc{};
    gmtime_r(&seconds, &utc);
    char text[32];
    std::strftime(text, sizeof text, "%Y-%m-%dT%H:%M:%SZ", &utc);
    return text;
}

}  // namespace

std::string identifier(const std::string& kind, const std::string& file) {
    uint64_t hash = 0xcbf29ce484222325ull;
    for (unsigned char byte : file) hash = (hash ^ byte) * 0x100000001b3ull;
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(hash));
    return "urn:aireader:" + kind + ":" + hex;
}

std::string proseLanguage(const std::vector<ChapterDraft>& chapters) {
    std::vector<PlainText> texts;
    for (const auto& chapter : chapters) {
        PlainText text;
        for (const auto& paragraph : chapter.paragraphs) text.text += paragraph.text + "\n";
        texts.push_back(std::move(text));
    }
    return LanguageDetector::detect(texts);
}

std::string epub(EpubBuilder::Metadata metadata, const std::vector<ChapterDraft>& chapters) {
    metadata.modified = now();
    ZipWriter zip;
    for (const auto& file : EpubBuilder::files(metadata, chapters)) zip.add(file.path, file.contents, file.path != "mimetype");
    return zip.finished();
}

std::string identifierOf(const std::string& path) {
    auto zip = ZipArchive::open(path);
    auto container = zip ? zip->find("META-INF/container.xml") : std::nullopt;
    auto containerXml = container ? zip->contents(*container) : std::nullopt;
    auto package = containerXml ? zip->contents(EpubPackage::packagePath(*containerXml)) : std::nullopt;
    if (!package) return "";
    std::string identifier, text;
    XmlScanner::Handlers handlers;
    handlers.onStart = [&](const std::string&, const XmlScanner::Attributes&) { text.clear(); };
    handlers.onText = [&](const std::string& chunk) { text += chunk; };
    handlers.onEnd = [&](const std::string& element) {
        if (element == "identifier" && identifier.empty()) identifier = Text::trim(text);
    };
    XmlScanner::scan(*package, handlers);
    return identifier;
}

std::string destination(const std::string& folder, const std::string& name, const std::string& identifier) {
    // Two files of one name, from different folders or edited since, make
    // two books; the same file added again finds the one made before. A book
    // that names no identifier cannot be told from another, so it never
    // counts as the same.
    std::vector<std::string> taken;
    while (true) {
        std::string path = Files::join(folder, RemoteBookName::make(name, "", taken));
        if (!Files::exists(path) || (!identifier.empty() && identifierOf(path) == identifier)) return path;
        taken.push_back(Files::baseName(path));
    }
}

std::string write(const std::string& path, const std::string& folder, const std::string& name, const std::string& kind,
                  const std::function<std::string(const std::string& file)>& make, std::string* error) {
    auto file = Files::read(path);
    if (!file) {
        if (error) *error = "Could not read " + path + ".";
        return "";
    }
    std::string destination = EpubWriter::destination(folder, name, identifier(kind, *file));
    if (Files::exists(destination)) return destination;
    try {
        if (!Files::write(destination, make(*file))) {
            if (error) *error = "Could not write " + destination + ".";
            return "";
        }
        return destination;
    } catch (const std::exception& failure) {
        if (error) *error = failure.what();
        return "";
    }
}

}  // namespace EpubWriter
