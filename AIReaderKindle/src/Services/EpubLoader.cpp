#include "EpubLoader.hpp"

#include "../Domain/Books/EpubNavigation.hpp"
#include "../Domain/Books/HtmlText.hpp"
#include "../Domain/Books/LanguageDetector.hpp"
#include "../Support/Files.hpp"
#include "../Support/Text.hpp"
#include "../Support/ZipArchive.hpp"

#include <glib.h>

#include <map>

namespace EpubLoader {

namespace {

struct Opened {
    ZipArchive archive;
    EpubPackage package;
    std::string packageDirectory;
};

std::optional<Opened> openPackage(const std::string& path, std::string* error) {
    std::string reason;
    auto archive = ZipArchive::open(path, &reason);
    if (!archive) {
        if (error) *error = reason;
        return std::nullopt;
    }
    auto containerPath = archive->find("META-INF/container.xml");
    auto container = containerPath ? archive->contents(*containerPath) : std::nullopt;
    std::string packagePath = container ? EpubPackage::packagePath(*container) : "";
    if (packagePath.empty()) {
        if (error) *error = "The EPUB has no META-INF/container.xml.";
        return std::nullopt;
    }
    auto opf = archive->contents(packagePath);
    if (!opf) {
        if (error) *error = "The EPUB has no package document.";
        return std::nullopt;
    }
    auto slash = packagePath.rfind('/');
    std::string directory = slash == std::string::npos ? "" : packagePath.substr(0, slash);
    return Opened{std::move(*archive), EpubPackage::parse(*opf), directory};
}

std::string unescape(const std::string& href) {
    gchar* unescaped = g_uri_unescape_string(href.c_str(), nullptr);
    std::string result = unescaped ? unescaped : href;
    g_free(unescaped);
    return result;
}

std::string directoryOf(const std::string& entry) {
    auto slash = entry.rfind('/');
    return slash == std::string::npos ? "" : entry.substr(0, slash);
}

/// The chapter's first heading, on one line, or its number.
std::string chapterName(const PlainText& chapter, int index) {
    for (const auto& span : chapter.spans) {
        if (span.kind != TextSpan::Kind::Heading) continue;
        std::string heading = chapter.text.substr(span.start, span.end - span.start);
        heading = Text::trim(heading.substr(0, heading.find('\n')));
        if (!heading.empty()) return heading;
    }
    return "Chapter " + std::to_string(index + 1);
}

/// The table of contents the book gives, pointed into the chapters kept —
/// an entry for a file dropped as blank, or never in the spine, is left
/// out. A book without one gets an entry per chapter.
std::vector<ContentsEntry> contents(
    const Opened& opened, const std::map<std::string, int>& chapterByEntry, const std::vector<PlainText>& chapters) {
    std::vector<ContentsEntry> entries;
    auto item = opened.package.navigationItem();
    std::string navPath = item ? EpubPackage::resolve(item->href, opened.packageDirectory) : "";
    auto markup = navPath.empty() ? std::nullopt : opened.archive.contents(navPath);
    for (const auto& nav : markup ? EpubNavigation::parse(*markup) : std::vector<NavEntry>{}) {
        auto hash = nav.href.find('#');
        std::string file = unescape(nav.href.substr(0, hash));
        std::string fragment = hash == std::string::npos ? "" : nav.href.substr(hash + 1);
        auto chapter = chapterByEntry.find(EpubPackage::resolve(file, directoryOf(navPath)));
        if (chapter == chapterByEntry.end()) continue;
        const auto& anchors = chapters[chapter->second].anchors;
        auto anchor = anchors.find(fragment);
        int offset = anchor == anchors.end() ? 0 : anchor->second;
        entries.push_back({nav.title, chapter->second, offset, nav.depth});
    }
    if (!entries.empty()) return entries;
    for (size_t i = 0; i < chapters.size(); ++i) {
        entries.push_back({chapterName(chapters[i], static_cast<int>(i)), static_cast<int>(i), 0, 0});
    }
    return entries;
}

}  // namespace

std::optional<Metadata> metadata(const std::string& path, std::string* error) {
    auto opened = openPackage(path, error);
    if (!opened) return std::nullopt;
    const EpubPackage& package = opened->package;
    return Metadata{
        package.title.empty() ? Files::stem(path) : package.title,
        package.author,
        package.language,
    };
}

std::optional<BookDocument> load(const std::string& path, std::string* error, bool withImages) {
    auto opened = openPackage(path, error);
    if (!opened) return std::nullopt;

    BookDocument document;
    document.language = opened->package.language;
    // Which chapter each file became, for the table of contents.
    std::map<std::string, int> chapterByEntry;
    for (const auto& item : opened->package.readingOrder()) {
        std::string entry = EpubPackage::resolve(item.href, opened->packageDirectory);
        auto markup = opened->archive.contents(entry);
        if (!markup) continue;
        PlainText chapter = HtmlText::plain(*markup);
        // Pictures are read out of the archive now, relative to the chapter.
        // Without them, a chapter is still kept or dropped the same way, so
        // chapter numbers agree between reading and searching.
        bool hasPictures = false;
        for (auto& image : chapter.images) {
            if (Text::contains(image.source, "://") || Text::startsWith(image.source, "data:")) continue;
            std::string imageEntry = EpubPackage::resolve(unescape(image.source), directoryOf(entry));
            if (withImages) image.bytes = opened->archive.contents(imageEntry).value_or("");
            hasPictures = hasPictures || (withImages ? !image.bytes.empty() : opened->archive.contains(imageEntry));
        }
        if (Text::isBlank(chapter.text) && !hasPictures) continue;
        chapterByEntry[entry] = static_cast<int>(document.chapters.size());
        document.chapters.push_back(std::move(chapter));
    }
    if (document.chapters.empty()) {
        if (error) *error = "The book has no readable content.";
        return std::nullopt;
    }
    document.contents = contents(*opened, chapterByEntry, document.chapters);
    // EPUB metadata is often wrong — plenty of French books declare
    // themselves English — so the prose has the final say.
    std::string detected = LanguageDetector::detect(document.chapters);
    document.language = detected.empty() ? LanguageDetector::code(document.language) : detected;
    return document;
}

std::optional<std::string> cover(const std::string& path) {
    auto opened = openPackage(path, nullptr);
    if (!opened) return std::nullopt;
    auto item = opened->package.coverItem();
    if (!item) return std::nullopt;
    return opened->archive.contents(EpubPackage::resolve(item->href, opened->packageDirectory));
}

}  // namespace EpubLoader
