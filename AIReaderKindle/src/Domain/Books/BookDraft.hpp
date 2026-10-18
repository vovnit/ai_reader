#pragma once

#include "EpubBuilder.hpp"

#include <string>
#include <vector>

/// A book read from a file of another format — FB2, plain text or HTML —
/// ready to be written as an EPUB (`EpubBuilder`). An empty author or
/// language means the file did not say.
struct BookDraft {
    std::string title;
    std::string author;
    std::string language;
    std::vector<ChapterDraft> chapters;
};
