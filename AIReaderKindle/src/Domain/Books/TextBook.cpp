#include "TextBook.hpp"

#include "../../Support/Text.hpp"
#include "Gutenberg.hpp"
#include "TextChapters.hpp"

#include <glib.h>

#include <algorithm>

namespace TextBook {

namespace {

/// The longest paragraph, in code points, still read as a chapter's heading.
constexpr long headingLength = 80;

// Words a chapter heading begins with, lowercased: English, French,
// Spanish, Italian, German, Russian, Ukrainian.
const char* const chapterWords[] = {
    "chapter", "part", "book", "prologue", "epilogue",
    "chapitre", "partie", "livre", "épilogue",
    "capítulo", "capitulo", "parte", "libro",
    "kapitel", "teil", "buch",
    "глава", "часть", "книга", "пролог", "эпилог", "розділ",
};

bool isDigits(const std::string& text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

bool isRoman(const std::string& text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return std::string("IVXLCDM").find(c) != std::string::npos; });
}

/// The lines, whether they end in LF, CRLF or CR, found in one pass:
/// replacing each CRLF in place would move the rest of the book every time.
std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> lines(1);
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c != '\r' && c != '\n') {
            lines.back() += c;
            continue;
        }
        if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
        lines.emplace_back();
    }
    return lines;
}

}  // namespace

bool opensChapter(const std::string& text) {
    if (g_utf8_strlen(text.c_str(), static_cast<gssize>(text.size())) > headingLength) return false;
    std::string number = Text::endsWith(text, ".") ? text.substr(0, text.size() - 1) : text;
    if (isDigits(number) || isRoman(number)) return true;
    std::string lower = Text::lower(text);
    return std::any_of(std::begin(chapterWords), std::end(chapterWords), [&](const std::string& word) {
        return lower == word || Text::startsWith(lower, word + " ");
    });
}

BookDraft read(const std::string& text, const std::string& fallbackTitle) {
    std::vector<std::string> all = lines(text);
    auto gutenberg = Gutenberg::parts(all);
    if (gutenberg) all = std::vector<std::string>(all.begin() + static_cast<long>(gutenberg->start) + 1, all.begin() + static_cast<long>(gutenberg->end));

    // Wrapped by hand when a blank line stands between two lines of text.
    auto first = std::find_if(all.begin(), all.end(), [](const std::string& line) { return !TextChapters::isBlank(line); });
    auto last = std::find_if(all.rbegin(), all.rend(), [](const std::string& line) { return !TextChapters::isBlank(line); }).base();
    bool wrapped = first < last && std::any_of(first, last, TextChapters::isBlank);

    std::vector<TextChapters::Paragraph> paragraphs;
    std::vector<std::string> run;
    auto end = [&]() {
        if (run.empty()) return;
        std::string paragraph = TextChapters::collapsed(Text::join(run, " "));
        bool opens = opensChapter(paragraph);
        paragraphs.push_back({paragraph, opens, opens});
        run.clear();
    };
    for (const auto& line : all) {
        if (TextChapters::isBlank(line)) {
            end();
        } else {
            run.push_back(line);
            if (!wrapped) end();
        }
    }
    end();

    BookDraft book;
    book.title = gutenberg && !gutenberg->title.empty() ? gutenberg->title : fallbackTitle;
    if (gutenberg) book.author = gutenberg->author;
    book.chapters = TextChapters::chapters(paragraphs, book.title);
    return book;
}

}  // namespace TextBook
