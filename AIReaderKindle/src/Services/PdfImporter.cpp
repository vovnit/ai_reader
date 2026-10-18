#include "PdfImporter.hpp"

#include "EpubWriter.hpp"

#include "../Domain/Books/LanguageDetector.hpp"
#include "../Domain/Books/PdfLayout.hpp"
#include "../Support/Files.hpp"
#include "../Support/PdfPageText.hpp"

#include <stdexcept>

namespace PdfImporter {

namespace {

const char* const noText = "The PDF has no text to read — it is probably a scan. ScanTool makes an EPUB of a scanned book from its OCR.";

std::string language(const PdfDocument& document, const std::vector<ChapterDraft>& chapters) {
    std::string detected = EpubWriter::proseLanguage(chapters);
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
    metadata.identifier = EpubWriter::identifier("pdf", pdf);
    return EpubWriter::epub(metadata, chapters);
}

std::string convert(const std::string& path, const std::string& folder, std::string* error) {
    std::string name = Files::stem(path);
    return EpubWriter::write(path, folder, name, [&name](const std::string& pdf) { return epub(pdf, name); }, error);
}

}  // namespace PdfImporter
