#include "HtmlBook.hpp"

#include "../../Support/Text.hpp"
#include "../../Support/XmlScanner.hpp"
#include "Gutenberg.hpp"
#include "TextChapters.hpp"

#include <set>

namespace HtmlBook {

namespace {

// Elements that end the paragraph before them and begin a new one.
const std::set<std::string> blocks = {
    "p", "div", "li", "dt", "dd", "blockquote", "pre", "td", "th", "caption", "figcaption", "figure",
    "section", "article", "header", "footer", "aside", "address", "main", "center", "hr",
    "h1", "h2", "h3", "h4", "h5", "h6", "tr", "table", "ul", "ol", "dl", "body",
};
// Elements whose text is not the book's.
const std::set<std::string> hidden = {"script", "style", "noscript", "template", "svg", "math", "nav"};
const std::set<std::string> headings = {"h1", "h2", "h3", "h4", "h5", "h6"};
// Elements a <head> holds; any other ends it, closed or not, as in a browser.
const std::set<std::string> headElements = {"title", "meta", "link", "style", "script", "base", "noscript", "template"};
// Elements whose content is not markup, so the scanner never sees it.
const char* const rawTextElements[] = {"script", "style"};

bool endsName(size_t i, const std::string& text) {
    if (i >= text.size()) return false;
    char c = text[i];
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '/' || c == '>';
}

/// `markup` without what lies inside `<script>` and `<style>`, where a "<"
/// would read as a tag that swallows the closing one, and the book after it.
std::string withoutRawText(const std::string& markup) {
    std::string lower = markup;
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    std::string out;
    size_t kept = 0;
    size_t i = 0;
    while ((i = lower.find('<', i)) != std::string::npos) {
        if (lower.compare(i, 4, "<!--") == 0) {
            size_t end = lower.find("-->", i + 4);
            i = end == std::string::npos ? lower.size() : end + 3;
            continue;
        }
        std::string name;
        for (const char* element : rawTextElements) {
            std::string candidate = element;
            if (lower.compare(i + 1, candidate.size(), candidate) == 0 && endsName(i + 1 + candidate.size(), lower)) name = candidate;
        }
        size_t open = name.empty() ? std::string::npos : lower.find('>', i);
        if (open == std::string::npos || lower[open - 1] == '/') {
            ++i;
            continue;
        }
        size_t close = lower.find("</" + name, open);
        out += markup.substr(kept, open + 1 - kept);
        kept = close == std::string::npos ? lower.size() : close;
        i = kept;
    }
    return out + markup.substr(kept);
}

}  // namespace

BookDraft read(const std::string& text, const std::string& fallbackTitle) {
    std::string title;
    std::string firstH1;
    std::string author;
    std::string language;
    std::vector<TextChapters::Paragraph> paragraphs;
    std::string gathered;
    std::string heading;
    bool inHead = false;
    bool inTitle = false;
    std::string titleText;
    // The hidden element being skipped, and how deep in ones of its name.
    std::string skipping;
    int depth = 0;

    auto flush = [&]() {
        std::string paragraph = TextChapters::collapsed(gathered);
        gathered.clear();
        if (TextChapters::isBlank(paragraph)) return;
        if (heading == "h1" && firstH1.empty()) firstH1 = paragraph;
        paragraphs.push_back({paragraph, !heading.empty(), heading == "h1" || heading == "h2"});
    };
    auto attribute = [](const XmlScanner::Attributes& attributes, const std::string& name) {
        auto found = attributes.find(name);
        return found == attributes.end() ? std::string() : found->second;
    };

    XmlScanner::Handlers handlers;
    handlers.onStart = [&](const std::string& name, const XmlScanner::Attributes& attributes) {
        if (!skipping.empty()) {
            if (name == skipping) ++depth;
            return;
        }
        if (!headElements.count(name)) inHead = false;
        if (hidden.count(name)) {
            skipping = name;
            depth = 1;
        } else if (name == "html") {
            language = Text::lower(TextChapters::collapsed(attribute(attributes, "lang")));
        } else if (name == "head") {
            inHead = true;
        } else if (name == "title") {
            inTitle = true;
            titleText.clear();
        } else if (name == "meta" && Text::lower(TextChapters::collapsed(attribute(attributes, "name"))) == "author" && author.empty()) {
            author = TextChapters::collapsed(attribute(attributes, "content"));
        } else if (name == "br") {
            // In a heading a break only sets the words on two lines:
            // "CHAPTER I.<br>Down the Rabbit-Hole" is one title.
            if (!heading.empty()) gathered += ' ';
            else flush();
        } else if (blocks.count(name)) {
            flush();
            if (headings.count(name)) heading = name;
        }
    };
    handlers.onEnd = [&](const std::string& name) {
        if (!skipping.empty()) {
            if (name == skipping && --depth == 0) skipping.clear();
            return;
        }
        if (name == "head") {
            inHead = false;
        } else if (name == "title" && inTitle) {
            inTitle = false;
            if (title.empty()) title = TextChapters::collapsed(titleText);
        } else if (blocks.count(name)) {
            flush();
            if (headings.count(name)) heading.clear();
        }
    };
    handlers.onText = [&](const std::string& run) {
        if (!skipping.empty()) return;
        if (inTitle) titleText += run;
        else if (!inHead) gathered += run;
    };
    XmlScanner::scan(withoutRawText(text), handlers);
    flush();

    std::vector<std::string> texts;
    for (const auto& paragraph : paragraphs) texts.push_back(paragraph.text);
    auto gutenberg = Gutenberg::parts(texts);
    if (gutenberg) paragraphs = std::vector<TextChapters::Paragraph>(paragraphs.begin() + static_cast<long>(gutenberg->start) + 1, paragraphs.begin() + static_cast<long>(gutenberg->end));

    BookDraft book;
    book.title = gutenberg && !gutenberg->title.empty() ? gutenberg->title
        : !title.empty() ? title
        : !firstH1.empty() ? firstH1
        : fallbackTitle;
    book.author = gutenberg && !gutenberg->author.empty() ? gutenberg->author : author;
    book.language = language;
    book.chapters = TextChapters::chapters(paragraphs, book.title);
    return book;
}

}  // namespace HtmlBook
