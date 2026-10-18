#include "TextChapters.hpp"

#include <algorithm>

namespace TextChapters {

namespace {

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

ChapterDraft::Paragraph bare(const Paragraph& paragraph) {
    return {paragraph.text, paragraph.heading};
}

}  // namespace

std::string collapsed(const std::string& text) {
    std::string out;
    bool space = false;
    for (char c : text) {
        if (isSpace(c)) {
            space = !out.empty();
            continue;
        }
        if (space) out += ' ';
        space = false;
        out += c;
    }
    return out;
}

bool isBlank(const std::string& text) {
    for (size_t i = 0; i < text.size(); ++i) {
        if (text.compare(i, 2, "\xC2\xA0") == 0) {
            ++i;
        } else if (!isSpace(text[i])) {
            return false;
        }
    }
    return true;
}

std::vector<ChapterDraft> chapters(const std::vector<Paragraph>& paragraphs, const std::string& title) {
    std::vector<ChapterDraft> chapters;
    if (std::any_of(paragraphs.begin(), paragraphs.end(), [](const Paragraph& paragraph) { return paragraph.opens; })) {
        chapters.push_back({title, {}});
        for (size_t i = 0; i < paragraphs.size(); ++i) {
            const Paragraph& paragraph = paragraphs[i];
            if (paragraph.opens && (i == 0 || !paragraphs[i - 1].opens)) {
                if (!chapters.back().paragraphs.empty()) chapters.push_back({paragraph.text, {}});
                else chapters.back().title = paragraph.text;
            }
            chapters.back().paragraphs.push_back(bare(paragraph));
        }
        return chapters;
    }
    for (size_t start = 0; start < paragraphs.size(); start += paragraphsPerPart) {
        ChapterDraft chapter;
        chapter.title = paragraphs.size() <= paragraphsPerPart ? title : "Part " + std::to_string(start / paragraphsPerPart + 1);
        for (size_t i = start; i < std::min(paragraphs.size(), start + paragraphsPerPart); ++i) chapter.paragraphs.push_back(bare(paragraphs[i]));
        chapters.push_back(chapter);
    }
    return chapters;
}

}  // namespace TextChapters
