#include "PdfLayout.hpp"

#include "PdfParagraphs.hpp"

#include <algorithm>

namespace PdfLayout {

std::vector<ChapterDraft> chapters(
    const std::vector<std::vector<PdfLine>>& pages,
    const std::vector<PdfBookmark>& bookmarks,
    const std::string& title)
{
    std::vector<PdfParagraph> paragraphs = PdfParagraphs::read(pages);
    size_t headings = std::count_if(paragraphs.begin(), paragraphs.end(), [](const PdfParagraph& paragraph) { return paragraph.heading; });
    std::vector<ChapterDraft> chapters;

    if (bookmarks.empty() && headings >= 2) {
        // No bookmarks, but headings: each run of them opens a chapter.
        chapters.push_back({title, {}});
        for (size_t i = 0; i < paragraphs.size(); ++i) {
            const PdfParagraph& paragraph = paragraphs[i];
            if (paragraph.heading && (i == 0 || !paragraphs[i - 1].heading)) {
                if (!chapters.back().paragraphs.empty()) chapters.push_back({paragraph.text, {}});
                else chapters.back().title = paragraph.text;
            }
            chapters.back().paragraphs.push_back({paragraph.text, paragraph.heading});
        }
        return chapters;
    }

    // A chapter from each bookmark's page, or from every few pages.
    std::vector<PdfBookmark> marks = bookmarks;
    std::stable_sort(marks.begin(), marks.end(), [](const PdfBookmark& a, const PdfBookmark& b) { return a.page < b.page; });
    marks.erase(std::unique(marks.begin(), marks.end(), [](const PdfBookmark& a, const PdfBookmark& b) { return a.page == b.page; }), marks.end());
    int count = static_cast<int>(pages.size());
    if (marks.empty()) {
        for (int start = 0; start < count; start += pagesPerChapter) {
            int end = std::min(count, start + pagesPerChapter);
            marks.push_back({count <= pagesPerChapter ? title : "Pages " + std::to_string(start + 1) + "–" + std::to_string(end), start});
        }
    } else if (marks.front().page > 0) {
        marks.insert(marks.begin(), {title, 0});
    }
    for (const auto& mark : marks) chapters.push_back({mark.title.empty() ? title : mark.title, {}});
    size_t chapter = 0;
    for (const auto& paragraph : paragraphs) {
        while (chapter + 1 < marks.size() && marks[chapter + 1].page <= paragraph.page) ++chapter;
        chapters[chapter].paragraphs.push_back({paragraph.text, paragraph.heading});
    }
    chapters.erase(std::remove_if(chapters.begin(), chapters.end(), [](const ChapterDraft& draft) { return draft.paragraphs.empty(); }), chapters.end());
    return chapters;
}

}  // namespace PdfLayout
