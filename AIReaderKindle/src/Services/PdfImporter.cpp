#include "PdfImporter.hpp"

#include "../Domain/Books/EpubBuilder.hpp"
#include "../Domain/Books/LanguageDetector.hpp"
#include "../Domain/Books/PdfLayout.hpp"
#include "../Support/Files.hpp"
#include "../Support/PdfPageText.hpp"
#include "../Support/ZipWriter.hpp"

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <stdexcept>

namespace PdfImporter {

namespace {

const char* const noText = "The PDF has no text to read — it is probably a scan. ScanTool makes an EPUB of a scanned book from its OCR.";

/// The same PDF makes a book with the same identifier.
std::string identifier(const std::string& pdf) {
    uint64_t hash = 0xcbf29ce484222325ull;
    for (unsigned char byte : pdf) hash = (hash ^ byte) * 0x100000001b3ull;
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(hash));
    return std::string("urn:aireader:pdf:") + hex;
}

std::string now() {
    std::time_t seconds = std::time(nullptr);
    std::tm utc{};
    gmtime_r(&seconds, &utc);
    char text[32];
    std::strftime(text, sizeof text, "%Y-%m-%dT%H:%M:%SZ", &utc);
    return text;
}

std::string language(const PdfDocument& document, const std::vector<ChapterDraft>& chapters) {
    std::vector<PlainText> texts;
    for (const auto& chapter : chapters) {
        PlainText text;
        for (const auto& paragraph : chapter.paragraphs) text.text += paragraph.text + "\n";
        texts.push_back(std::move(text));
    }
    std::string detected = LanguageDetector::detect(texts);
    if (detected.empty()) detected = LanguageDetector::code(document.language());
    return detected.empty() ? "und" : detected;
}

}  // namespace

std::string epub(const std::string& pdf, const std::string& fallbackTitle) {
    PdfDocument document(pdf);
    PdfPageText::FontCache fonts;
    std::vector<std::vector<PdfLine>> pages;
    for (const auto& page : document.pages()) pages.push_back(PdfPageText::lines(document, page, fonts));

    EpubBuilder::Metadata metadata;
    metadata.title = document.info("Title");
    if (metadata.title.empty()) metadata.title = fallbackTitle;
    metadata.author = document.info("Author");
    std::vector<ChapterDraft> chapters = PdfLayout::chapters(pages, document.outline(), metadata.title);
    if (chapters.empty()) throw std::runtime_error(noText);
    metadata.language = language(document, chapters);
    metadata.identifier = identifier(pdf);
    metadata.modified = now();

    ZipWriter zip;
    for (const auto& file : EpubBuilder::files(metadata, chapters)) zip.add(file.path, file.contents, file.path != "mimetype");
    return zip.finished();
}

std::string convert(const std::string& path, const std::string& folder, std::string* error) {
    std::string destination = Files::join(folder, Files::stem(path) + ".epub");
    if (Files::exists(destination)) return destination;
    auto pdf = Files::read(path);
    if (!pdf) {
        if (error) *error = "Could not read " + path + ".";
        return "";
    }
    try {
        if (!Files::write(destination, epub(*pdf, Files::stem(path)))) {
            if (error) *error = "Could not write " + destination + ".";
            return "";
        }
        return destination;
    } catch (const std::exception& failure) {
        if (error) *error = failure.what();
        return "";
    }
}

}  // namespace PdfImporter
