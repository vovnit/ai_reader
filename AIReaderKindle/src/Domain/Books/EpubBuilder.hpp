#pragma once

#include <string>
#include <vector>

/// A chapter to be written into an EPUB: its title for the table of
/// contents, and its paragraphs, some of them headings.
struct ChapterDraft {
    struct Paragraph {
        std::string text;
        bool heading = false;
    };
    std::string title;
    std::vector<Paragraph> paragraphs;
};

/// One file of an EPUB, by its path in the archive.
struct EpubFile {
    std::string path;
    std::string contents;
};

/// Writes a book of plain chapters as an EPUB 3 with an EPUB 2 table of
/// contents too — the shape of ScanTool's books and the browser
/// extension's.
namespace EpubBuilder {

struct Metadata {
    std::string title;
    std::string author;
    std::string language;
    std::string identifier;
    /// When the book was made, as `2026-10-06T12:00:00Z`.
    std::string modified;
};

/// The book's files, `mimetype` first.
std::vector<EpubFile> files(const Metadata& metadata, const std::vector<ChapterDraft>& chapters);

}  // namespace EpubBuilder
