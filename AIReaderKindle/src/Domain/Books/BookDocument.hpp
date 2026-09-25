#pragma once

#include "HtmlText.hpp"

#include <string>
#include <vector>

/// One line of the table of contents, pointing into the loaded chapters.
struct ContentsEntry {
    std::string title;
    int chapter = 0;
    int offset = 0;
    /// 0 for a top-level entry; an entry nested under another is deeper.
    int depth = 0;
};

/// A book loaded for reading: one plain-text chapter per spine item.
struct BookDocument {
    std::vector<PlainText> chapters;
    /// The table of contents as the book gives it, or one entry per
    /// chapter when it gives none.
    std::vector<ContentsEntry> contents;
    /// What the EPUB declares itself to be written in, e.g. "fr".
    std::string language;

    const PlainText& chapter(int index) const {
        static const PlainText empty;
        return index >= 0 && index < static_cast<int>(chapters.size()) ? chapters[index] : empty;
    }

    /// The entry the reader is in: the one nearest before a place, or -1
    /// before the first. Entries are read as the book lists them, which is
    /// nearly always in order but need not be.
    int contentsEntryAt(int chapter, int offset) const {
        int found = -1;
        for (size_t i = 0; i < contents.size(); ++i) {
            const ContentsEntry& entry = contents[i];
            if (entry.chapter > chapter || (entry.chapter == chapter && entry.offset > offset)) continue;
            if (found < 0 || entry.chapter > contents[found].chapter
                || (entry.chapter == contents[found].chapter && entry.offset >= contents[found].offset)) {
                found = static_cast<int>(i);
            }
        }
        return found;
    }
};
