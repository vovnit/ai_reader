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
/// As long again for a chapter word and its number, which may run on into
/// the chapter's title.
constexpr long numberedHeadingLength = 160;
/// A line longer than this, in code points, is a paragraph on one line, not
/// wrapped by hand.
constexpr long wrappedLineLength = 100;

// Words a chapter heading begins with, lowercased: English, French,
// Spanish, Italian, German, Russian, Ukrainian.
const char* const chapterWords[] = {
    "chapter", "part", "book", "prologue", "epilogue",
    "chapitre", "partie", "livre", "épilogue",
    "capítulo", "capitulo", "parte", "libro",
    "kapitel", "teil", "buch",
    "глава", "часть", "книга", "пролог", "эпилог", "розділ",
};

/// How many letters of `text`, from `i`, one place of a roman numeral takes
/// when written the standard way: up to three `one`s, `one` and `five`,
/// `five` and up to three `one`s, or `one` and `ten`.
size_t romanPlace(const std::string& text, size_t i, char one, char five, char ten) {
    auto at = [&](size_t j) { return j < text.size() ? text[j] : '\0'; };
    if (at(i) == one && (at(i + 1) == five || at(i + 1) == ten)) return 2;
    size_t start = at(i) == five ? 1 : 0;
    size_t ones = 0;
    while (ones < 3 && at(i + start + ones) == one) ++ones;
    return start + ones;
}

/// "I" to "MMMCMXCIX", written the standard way: "DID" and "CIVIL" are
/// words, not numbers.
bool isRoman(const std::string& text) {
    size_t i = 0;
    while (i < 3 && i < text.size() && text[i] == 'M') ++i;
    i += romanPlace(text, i, 'C', 'D', 'M');
    i += romanPlace(text, i, 'X', 'L', 'C');
    i += romanPlace(text, i, 'I', 'V', 'X');
    return !text.empty() && i == text.size();
}

/// A chapter's number: up to three digits, or an upper-case roman numeral.
bool isNumber(const std::string& text) {
    bool digits = !text.empty() && text.size() <= 3 && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
    return digits || isRoman(text);
}

/// "I.", "12:", "IV—The Return": a number, then nothing or a stop, colon or
/// dash and anything.
bool startsWithNumber(const std::string& word) {
    size_t end = 0;
    while (end < word.size() && word[end] != '.' && word[end] != ':' && word[end] != '-' && word.compare(end, 3, "\xE2\x80\x94") != 0) ++end;
    return isNumber(word.substr(0, end));
}

/// The words of `text`, parted by spaces and, as French sets one before
/// ":", no-break spaces.
std::vector<std::string> words(const std::string& text) {
    std::vector<std::string> words(1);
    for (size_t i = 0; i < text.size(); ++i) {
        bool space = text[i] == ' ' || text.compare(i, 2, "\xC2\xA0") == 0;
        if (!space) {
            words.back() += text[i];
            continue;
        }
        if (text[i] != ' ') ++i;
        if (!words.back().empty()) words.emplace_back();
    }
    if (words.back().empty()) words.pop_back();
    return words;
}

/// Ends as a sentence does: in a stop, "!", "?" or "…", perhaps inside
/// quotation marks.
bool endsSentence(const std::string& text) {
    static const char* const quotes[] = {"\"", "'", "\xE2\x80\x99", "\xE2\x80\x9D", "\xC2\xBB"};
    std::string rest = text;
    for (bool quoted = true; quoted;) {
        quoted = false;
        for (const char* quote : quotes) {
            if (Text::endsWith(rest, quote)) {
                rest.resize(rest.size() - std::string(quote).size());
                quoted = true;
            }
        }
    }
    return Text::endsWith(rest, ".") || Text::endsWith(rest, "!") || Text::endsWith(rest, "?") || Text::endsWith(rest, "\xE2\x80\xA6");
}

bool hasLowerCase(const std::string& text) {
    for (const char* c = text.data(); c < text.data() + text.size(); c = g_utf8_next_char(c)) {
        if (g_unichar_islower(g_utf8_get_char(c))) return true;
    }
    return false;
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
    long length = g_utf8_strlen(text.c_str(), static_cast<gssize>(text.size()));
    if (length > numberedHeadingLength) return false;
    std::vector<std::string> parts = words(text);
    if (parts.empty()) return false;
    std::string first = Text::lower(parts[0]);
    bool isWord = std::find(std::begin(chapterWords), std::end(chapterWords), first) != std::end(chapterWords);
    if (isWord && parts.size() > 1 && startsWithNumber(parts[1])) return true;
    if (length > headingLength) return false;
    if (isNumber(Text::endsWith(text, ".") ? text.substr(0, text.size() - 1) : text)) return true;
    if (parts.size() == 1) {
        return std::any_of(std::begin(chapterWords), std::end(chapterWords), [&](const std::string& word) {
            return first == word || first == word + "." || first == word + ":";
        });
    }
    bool lower = hasLowerCase(text);
    if (isWord) return !lower || !endsSentence(text);
    size_t end = 0;
    while (end < parts[0].size() && parts[0][end] != '.' && parts[0].compare(end, 3, "\xE2\x80\x94") != 0) ++end;
    return !lower && end < parts[0].size() && isNumber(parts[0].substr(0, end));
}

BookDraft read(const std::string& text, const std::string& fallbackTitle) {
    std::vector<std::string> all = lines(text);
    auto gutenberg = Gutenberg::parts(all);
    if (gutenberg) all = std::vector<std::string>(all.begin() + static_cast<long>(gutenberg->start) + 1, all.begin() + static_cast<long>(gutenberg->end));

    // Wrapped by hand when a blank line stands between two lines of text and
    // fewer than one line in ten is longer than wrapping would leave it:
    // lib.ru sets a paragraph on one line, and a blank line between some.
    size_t filled = 0, blank = 0, longLines = 0;
    auto first = std::find_if(all.begin(), all.end(), [](const std::string& line) { return !TextChapters::isBlank(line); });
    auto last = std::find_if(all.rbegin(), all.rend(), [](const std::string& line) { return !TextChapters::isBlank(line); }).base();
    for (auto line = first; line < last; ++line) {
        if (TextChapters::isBlank(*line)) {
            ++blank;
        } else {
            ++filled;
            if (static_cast<long>(line->size()) > wrappedLineLength && g_utf8_strlen(line->c_str(), static_cast<gssize>(line->size())) > wrappedLineLength) ++longLines;
        }
    }
    bool wrapped = blank > 0 && longLines * 10 < filled;

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
