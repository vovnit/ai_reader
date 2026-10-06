#include "EpubBuilder.hpp"

namespace EpubBuilder {

namespace {

std::string escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c;
        }
    }
    return out;
}

std::string chapterPath(size_t number) {
    return "text/chapter-" + std::to_string(number) + ".xhtml";
}

const char* const container =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
    "<rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles>\n"
    "</container>\n";

std::string package(const Metadata& book, size_t count) {
    std::string items =
        "<item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>\n"
        "<item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>\n"
        "<item id=\"style\" href=\"style.css\" media-type=\"text/css\"/>\n";
    std::string spine;
    for (size_t n = 1; n <= count; ++n) {
        items += "<item id=\"chapter-" + std::to_string(n) + "\" href=\"" + chapterPath(n) + "\" media-type=\"application/xhtml+xml\"/>\n";
        spine += "<itemref idref=\"chapter-" + std::to_string(n) + "\"/>\n";
    }
    std::string creator = book.author.empty() ? "" : "<dc:creator>" + escape(book.author) + "</dc:creator>\n";
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" unique-identifier=\"uid\">\n"
        "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
        "<dc:identifier id=\"uid\">" + escape(book.identifier) + "</dc:identifier>\n"
        "<dc:title>" + escape(book.title) + "</dc:title>\n"
        + creator + "<dc:language>" + escape(book.language) + "</dc:language>\n"
        "<meta property=\"dcterms:modified\">" + book.modified + "</meta>\n"
        "</metadata>\n<manifest>\n" + items + "</manifest>\n<spine toc=\"ncx\">\n" + spine + "</spine>\n</package>\n";
}

std::string navigation(const Metadata& book, const std::vector<ChapterDraft>& chapters) {
    std::string entries;
    for (size_t n = 1; n <= chapters.size(); ++n) {
        entries += "<li><a href=\"" + chapterPath(n) + "\">" + escape(chapters[n - 1].title) + "</a></li>\n";
    }
    std::string language = escape(book.language);
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE html>\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\" lang=\"" + language + "\" xml:lang=\"" + language + "\">\n"
        "<head><title>" + escape(book.title) + "</title></head>\n"
        "<body><nav epub:type=\"toc\"><h1>" + escape(book.title) + "</h1><ol>\n" + entries + "</ol></nav></body>\n</html>\n";
}

std::string ncx(const Metadata& book, const std::vector<ChapterDraft>& chapters) {
    std::string points;
    for (size_t n = 1; n <= chapters.size(); ++n) {
        std::string number = std::to_string(n);
        points += "<navPoint id=\"p" + number + "\" playOrder=\"" + number + "\"><navLabel><text>" + escape(chapters[n - 1].title)
            + "</text></navLabel><content src=\"" + chapterPath(n) + "\"/></navPoint>\n";
    }
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<ncx xmlns=\"http://www.daisy.org/z3986/2005/ncx/\" version=\"2005-1\">\n"
        "<head><meta name=\"dtb:uid\" content=\"" + escape(book.identifier) + "\"/></head>\n"
        "<docTitle><text>" + escape(book.title) + "</text></docTitle>\n<navMap>\n" + points + "</navMap>\n</ncx>\n";
}

std::string chapter(const Metadata& book, const ChapterDraft& draft) {
    std::string body;
    for (const auto& paragraph : draft.paragraphs) {
        const char* tag = paragraph.heading ? "h2" : "p";
        body += std::string("<") + tag + ">" + escape(paragraph.text) + "</" + tag + ">\n";
    }
    std::string language = escape(book.language);
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE html>\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\" lang=\"" + language + "\" xml:lang=\"" + language + "\">\n"
        "<head><title>" + escape(draft.title) + "</title><link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\"/></head>\n"
        "<body>\n" + body + "</body>\n</html>\n";
}

}  // namespace

std::vector<EpubFile> files(const Metadata& metadata, const std::vector<ChapterDraft>& chapters) {
    std::vector<EpubFile> files = {
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml", container},
        {"OEBPS/content.opf", package(metadata, chapters.size())},
        {"OEBPS/nav.xhtml", navigation(metadata, chapters)},
        {"OEBPS/toc.ncx", ncx(metadata, chapters)},
        {"OEBPS/style.css", "body { line-height: 1.5; }\n"},
    };
    for (size_t n = 1; n <= chapters.size(); ++n) files.push_back({"OEBPS/" + chapterPath(n), chapter(metadata, chapters[n - 1])});
    return files;
}

}  // namespace EpubBuilder
