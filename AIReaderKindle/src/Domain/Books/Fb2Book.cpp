#include "Fb2Book.hpp"

#include "../../Support/Text.hpp"
#include "../../Support/XmlScanner.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace Fb2Book {

namespace {

// Each is one paragraph: a <p> in a <title> is a heading, as is a
// <subtitle>; a table row is its cells joined.
const std::set<std::string> paragraphElements = {"p", "v", "subtitle", "text-author", "date", "tr"};
// Each ends the paragraph before it, as the start or end of a paragraph does.
const std::set<std::string> blocks = {
    "section", "title", "epigraph", "poem", "stanza", "cite", "annotation", "table",
    "body", "empty-line", "image", "binary", "stylesheet",
};
// Never hold anything, so they are not kept open even when written without "/>".
const std::set<std::string> empty = {"empty-line", "image"};
const char* const nameParts[] = {"first-name", "middle-name", "last-name"};

/// Every run of ASCII whitespace made one space, none at the ends; a
/// no-break space stays.
std::string collapsed(const std::string& text) {
    std::string out;
    bool space = false;
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            space = !out.empty();
            continue;
        }
        if (space) out += ' ';
        space = false;
        out += c;
    }
    return out;
}

/// The paragraph being read, as cells: one, or a table row's.
struct Paragraph {
    std::vector<std::string> cells;
    bool heading = false;
    bool row = false;
};

class Reader {
public:
    void start(const std::string& name);
    void end(const std::string& name);
    void text(const std::string& run);
    BookDraft book(const std::string& fallbackTitle);

private:
    // The open elements, and how many of each: an end tag closes everything
    // opened after its element, and one that matches nothing is ignored.
    std::vector<std::string> stack;
    std::map<std::string, int> counts;

    // The <title-info> fields, each its first element's text.
    std::map<std::string, std::string> fields;
    std::string field;
    int authors = 0;

    int bodies = 0;
    std::vector<ChapterDraft> chapters;
    std::vector<ChapterDraft> notes;
    std::optional<Paragraph> paragraph;
    // The lines of the title naming a chapter: a section's in the first
    // body, the body's own in a later one.
    std::optional<std::vector<std::string>> titleLines;

    bool inside(const std::string& name) const {
        auto found = counts.find(name);
        return found != counts.end() && found->second > 0;
    }
    bool reading() const { return inside("body") && !inside("binary") && !inside("stylesheet"); }
    ChapterDraft& chapter() { return bodies > 1 ? notes.back() : chapters.back(); }
    void flush();
    void begin(bool heading, bool row);
    void close(const std::string& name);
};

void Reader::flush() {
    if (!paragraph) return;
    std::vector<std::string> cells;
    for (const auto& cell : paragraph->cells) {
        std::string line = collapsed(cell);
        if (!line.empty()) cells.push_back(line);
    }
    bool heading = paragraph->heading;
    paragraph.reset();
    std::string line = Text::join(cells, " | ");
    if (line.empty()) return;
    if (titleLines) titleLines->push_back(line);
    chapter().paragraphs.push_back({line, heading});
}

void Reader::begin(bool heading, bool row) {
    flush();
    paragraph = Paragraph{{""}, heading, row};
}

void Reader::start(const std::string& name) {
    if (inside("title-info") && !inside("body")) {
        if (name == "author") ++authors;
        bool namePart = name == "nickname" || std::find(std::begin(nameParts), std::end(nameParts), name) != std::end(nameParts);
        bool wanted = namePart ? inside("author") && authors == 1 : name == "book-title" || name == "lang";
        if (wanted && !fields.count(name)) {
            fields[name] = "";
            field = name;
        }
    }
    if (blocks.count(name)) flush();
    if (name == "body") {
        ++bodies;
        if (bodies == 1) chapters.push_back({"", {}});
        else notes.push_back({"Notes", {}});
    } else if (reading()) {
        if (name == "section") {
            // A section opens a chapter, unless the one before has nothing but
            // headings yet: then "Part One" and "Chapter 1" open one together.
            if (bodies == 1) {
                const auto& lines = chapter().paragraphs;
                if (std::any_of(lines.begin(), lines.end(), [](const ChapterDraft::Paragraph& line) { return !line.heading; })) {
                    chapters.push_back({"", {}});
                }
            }
        } else if (name == "title") {
            if (!titleLines && !stack.empty() && stack.back() == (bodies == 1 ? "section" : "body")) titleLines.emplace();
        } else if (paragraphElements.count(name)) {
            begin(inside("title") || name == "subtitle", name == "tr");
        } else if (name == "td" || name == "th") {
            if (!paragraph || !paragraph->row) begin(inside("title"), true);
            paragraph->cells.push_back("");
        }
    }
    if (empty.count(name)) return;
    stack.push_back(name);
    ++counts[name];
}

void Reader::close(const std::string& name) {
    if (name == field) field.clear();
    if (blocks.count(name) || paragraphElements.count(name)) flush();
    if (name == "title" && titleLines) {
        if (!titleLines->empty()) chapter().title = Text::join(*titleLines, " ");
        titleLines.reset();
    }
}

void Reader::end(const std::string& name) {
    if (!inside(name)) return;
    for (;;) {
        std::string closed = stack.back();
        stack.pop_back();
        --counts[closed];
        close(closed);
        if (closed == name) return;
    }
}

void Reader::text(const std::string& run) {
    if (reading()) {
        // Text outside any paragraph still reads, as one of its own.
        if (!paragraph) {
            if (collapsed(run).empty()) return;
            paragraph = Paragraph{{""}, inside("title"), false};
        }
        paragraph->cells.back() += run;
    } else if (!field.empty()) {
        fields[field] += run;
    }
}

BookDraft Reader::book(const std::string& fallbackTitle) {
    // A file cut short reads as if everything open closed there.
    while (!stack.empty()) {
        std::string last = stack.back();
        end(last);
    }
    auto part = [&](const std::string& name) {
        auto found = fields.find(name);
        return found == fields.end() ? std::string() : collapsed(found->second);
    };
    BookDraft book;
    book.title = part("book-title");
    if (book.title.empty()) book.title = fallbackTitle;
    std::vector<std::string> names;
    for (const char* name : nameParts) {
        std::string text = part(name);
        if (!text.empty()) names.push_back(text);
    }
    book.author = names.empty() ? part("nickname") : Text::join(names, " ");
    book.language = Text::lower(part("lang"));
    for (auto* drafts : {&chapters, &notes}) {
        for (auto& draft : *drafts) {
            if (draft.paragraphs.empty()) continue;
            if (draft.title.empty()) draft.title = book.title;
            book.chapters.push_back(std::move(draft));
        }
    }
    return book;
}

}  // namespace

BookDraft read(const std::string& text, const std::string& fallbackTitle) {
    Reader reader;
    XmlScanner::scan(text, {
        [&](const std::string& name, const XmlScanner::Attributes&) { reader.start(name); },
        [&](const std::string& name) { reader.end(name); },
        [&](const std::string& run) { reader.text(run); },
    });
    return reader.book(fallbackTitle);
}

}  // namespace Fb2Book
