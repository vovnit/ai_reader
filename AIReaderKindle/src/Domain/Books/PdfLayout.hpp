#pragma once

#include "../../Support/PdfDocument.hpp"
#include "../../Support/PdfPageText.hpp"
#include "EpubBuilder.hpp"

#include <string>
#include <vector>

/// Makes a PDF's text into a book's chapters.
namespace PdfLayout {

/// How many pages make a chapter when the PDF has no bookmarks to say.
constexpr int pagesPerChapter = 10;

/// One chapter per bookmark, the pages before the first making one of
/// their own. Without bookmarks, each heading opens a chapter, and without
/// headings either, every `pagesPerChapter` pages do. `title` names a
/// chapter nothing else names. Chapters with no text are left out.
std::vector<ChapterDraft> chapters(
    const std::vector<std::vector<PdfLine>>& pages,
    const std::vector<PdfBookmark>& bookmarks,
    const std::string& title);

}  // namespace PdfLayout
