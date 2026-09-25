#pragma once

#include <string>
#include <vector>

/// One line of a book's table of contents, as the EPUB lists it.
struct NavEntry {
    std::string title;
    /// As written: relative to the navigation document, perhaps with a
    /// `#fragment` naming a place inside the file.
    std::string href;
    /// 0 for a top-level entry; an entry nested under another is deeper.
    int depth = 0;
};

/// Reads the table of contents out of an EPUB's navigation document: the NCX
/// of EPUB 2, or the `<nav epub:type="toc">` of EPUB 3.
namespace EpubNavigation {

std::vector<NavEntry> fromNcx(const std::string& xml);
std::vector<NavEntry> fromNav(const std::string& xhtml);
/// Whichever the document is; an NCX has an `<ncx>` root.
std::vector<NavEntry> parse(const std::string& markup);

}  // namespace EpubNavigation
