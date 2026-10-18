#pragma once

#include "EpubBuilder.hpp"

#include <string>
#include <vector>

/// Makes the paragraphs of a plain-text or HTML book into its chapters: each
/// run of paragraphs that open a chapter starts one, titled by the run's
/// last two joined with " — ", and the paragraphs before the first make one
/// titled with the book's title. A book where nothing opens a chapter is cut
/// every `paragraphsPerPart` paragraphs. Chapters with no paragraphs are
/// left out. The same as the web app's `TextChapters.js` and the iOS app's
/// `TextChapters.swift`.
///
/// Books are matched across devices by title and author, and reading places
/// by chapter number and text, so the three apps must read a file to the
/// same strings: hence one whitespace rule, here, for both readers.
namespace TextChapters {

/// How many paragraphs make a chapter when nothing in the book opens one.
constexpr size_t paragraphsPerPart = 100;

struct Paragraph {
    std::string text;
    bool heading = false;
    bool opens = false;
};

/// Runs of space, tab, CR and LF made one space and trimmed; a no-break
/// space, which French sets before ":", stays.
std::string collapsed(const std::string& text);

/// True when nothing but those spaces and no-break spaces is there: an
/// `&nbsp;` keeps a paragraph open in HTML but says nothing.
bool isBlank(const std::string& text);

std::vector<ChapterDraft> chapters(const std::vector<Paragraph>& paragraphs, const std::string& title);

}  // namespace TextChapters
