#include "EpubWriter.hpp"

#include "../Domain/Books/LanguageDetector.hpp"
#include "../Support/Files.hpp"
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

std::string write(const std::string& path, const std::string& folder, const std::string& name,
                  const std::function<std::string(const std::string& file)>& make, std::string* error) {
    std::string destination = Files::join(folder, name + ".epub");
    if (Files::exists(destination)) return destination;
    auto file = Files::read(path);
    if (!file) {
        if (error) *error = "Could not read " + path + ".";
        return "";
    }
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
