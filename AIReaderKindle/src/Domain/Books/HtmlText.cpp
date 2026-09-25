#include "HtmlText.hpp"

#include "../../Support/Text.hpp"
#include "../../Support/XmlScanner.hpp"

#include <glib.h>

#include <algorithm>
#include <optional>
#include <set>

const char* const imagePlaceholder = "\xEF\xBF\xBC";  // U+FFFC

namespace HtmlText {

namespace {

const std::set<std::string> blocks = {
    "p", "div", "h1", "h2", "h3", "h4", "h5", "h6", "li", "ul", "ol", "tr", "table",
    "blockquote", "section", "article", "header", "footer", "aside", "hr", "pre",
    "dd", "dt", "dl", "figure", "figcaption", "body", "nav", "address",
};
const std::set<std::string> skipped = {"head", "script", "style", "title", "svg"};
const std::set<std::string> headings = {"h1", "h2", "h3", "h4", "h5", "h6"};
const std::set<std::string> bold = {"b", "strong"};
const std::set<std::string> italic = {"i", "em", "cite", "dfn"};

std::string attribute(const XmlScanner::Attributes& attributes, const char* key) {
    auto found = attributes.find(key);
    return found == attributes.end() ? std::string() : found->second;
}

/// True for the text of a footnote reference: a number or a sign such as *
/// or †, perhaps in brackets — nothing a reader would read as a word.
bool isNoteMark(const std::string& text) {
    if (text.empty() || g_utf8_strlen(text.c_str(), -1) > 5) return false;
    for (const char* p = text.c_str(); *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        bool sign = c == '*' || c == 0x2020 /* † */ || c == 0x2021 /* ‡ */ || c == 0xA7 /* § */
            || c == 0x21A9 /* ↩ */ || c == '[' || c == ']' || c == '(' || c == ')';
        if (!g_unichar_isdigit(c) && !sign) return false;
    }
    return true;
}

/// What an element is for, as EPUB 3 says it: its epub:type and ARIA role.
std::string purpose(const XmlScanner::Attributes& attributes) {
    return attribute(attributes, "type") + " " + attribute(attributes, "role");
}

/// Elements a reading system does not show: those marked hidden, such as the
/// landmarks of a navigation document, and print page numbers.
bool isHidden(const XmlScanner::Attributes& attributes) {
    return attributes.count("hidden") || Text::contains(purpose(attributes), "pagebreak");
}

struct Builder {
    PlainText out;
    /// The element being skipped, with how deep the same name is nested.
    std::string skipName;
    int skipDepth = 0;
    int preDepth = 0;
    bool pendingSpace = false;
    std::vector<std::pair<std::string, TextSpan>> openSpans;
    /// The link being read: where its text starts, and whether it is a note
    /// reference by declaration, or could be one by the look of its text.
    struct Link { int start; bool pendingSpace; bool declared; bool mayBeNote; };
    std::optional<Link> link;

    void breakParagraph() {
        pendingSpace = false;
        if (!out.text.empty() && out.text.back() != '\n') out.text += '\n';
    }

    void openSpan(const std::string& name, TextSpan::Kind kind) {
        // A space owed before the span belongs outside it.
        if (pendingSpace) out.text += ' ';
        pendingSpace = false;
        openSpans.push_back({name, TextSpan{static_cast<int>(out.text.size()), 0, kind}});
    }

    void closeSpan(const std::string& name) {
        for (auto it = openSpans.rbegin(); it != openSpans.rend(); ++it) {
            if (it->first != name) continue;
            TextSpan span = it->second;
            span.end = static_cast<int>(out.text.size());
            if (span.end > span.start) out.spans.push_back(span);
            openSpans.erase(std::next(it).base());
            return;
        }
    }

    void image(const std::string& source) {
        if (source.empty()) return;
        breakParagraph();
        out.images.push_back({static_cast<int>(out.text.size()), source, ""});
        out.text += imagePlaceholder;
        breakParagraph();
    }

    void openLink(const XmlScanner::Attributes& attributes) {
        bool declared = Text::contains(purpose(attributes), "noteref");
        // A mark at the head of a paragraph is a note's own number, not a
        // reference to one; that stays.
        bool midParagraph = !out.text.empty() && out.text.back() != '\n';
        bool toFragment = Text::contains(attribute(attributes, "href"), "#");
        link = Link{static_cast<int>(out.text.size()), pendingSpace, declared, midParagraph && toFragment};
    }

    /// A footnote reference is dropped: the reader cannot follow it, and a
    /// number glued to a word spoils looking the word up.
    void closeLink() {
        if (!link) return;
        Link closed = *link;
        link.reset();
        bool looksLike = closed.mayBeNote && isNoteMark(Text::trim(out.text.substr(closed.start)));
        if (!closed.declared && !looksLike) return;
        out.text.erase(closed.start);
        pendingSpace = closed.pendingSpace;
        while (!out.spans.empty() && out.spans.back().start >= closed.start) out.spans.pop_back();
        for (auto& open : openSpans) open.second.start = std::min(open.second.start, closed.start);
    }

    void start(const std::string& name, const XmlScanner::Attributes& attributes) {
        // Pictures come as <img src> and, inside an <svg>, as <image href>.
        if (name == "img" && !skipDepth) { image(attribute(attributes, "src")); return; }
        if (name == "image") { image(attribute(attributes, "href")); return; }
        if (skipDepth) { if (name == skipName) ++skipDepth; return; }
        if (skipped.count(name) || isHidden(attributes)) { skipName = name; skipDepth = 1; return; }
        if (name == "br") { out.text += '\n'; pendingSpace = false; return; }
        // Cells of one row stay on one line, a space apart.
        if (name == "td" || name == "th") { if (!out.text.empty() && out.text.back() != '\n') pendingSpace = true; return; }
        if (name == "pre") ++preDepth;
        if (blocks.count(name)) breakParagraph();
        // Recorded after the break, so the anchor is the paragraph's start.
        std::string id = attribute(attributes, "id");
        if (id.empty() && name == "a") id = attribute(attributes, "name");
        if (!id.empty()) out.anchors.emplace(id, static_cast<int>(out.text.size()));
        // A rule is a scene break: a blank line, since paragraphs are only indented.
        if (name == "hr" && !out.text.empty()) out.text += '\n';
        if (name == "a") openLink(attributes);
        if (name == "q") text("\xE2\x80\x9C");
        if (headings.count(name)) openSpan(name, TextSpan::Kind::Heading);
        else if (bold.count(name)) openSpan(name, TextSpan::Kind::Bold);
        else if (italic.count(name)) openSpan(name, TextSpan::Kind::Italic);
        else if (name == "sup") openSpan(name, TextSpan::Kind::Superscript);
        else if (name == "sub") openSpan(name, TextSpan::Kind::Subscript);
    }

    void end(const std::string& name) {
        if (skipDepth) { if (name == skipName) --skipDepth; return; }
        if (name == "pre" && preDepth) --preDepth;
        if (name == "q") { pendingSpace = false; out.text += "\xE2\x80\x9D"; }
        if (headings.count(name) || bold.count(name) || italic.count(name) || name == "sup" || name == "sub") {
            closeSpan(name);
        }
        if (name == "a") closeLink();
        if (blocks.count(name)) breakParagraph();
    }

    void text(const std::string& chunk) {
        if (skipDepth) return;
        if (preDepth) { out.text += chunk; return; }
        for (size_t i = 0; i < chunk.size(); ++i) {
            char c = chunk[i];
            bool space = c == ' ' || c == '\t' || c == '\r' || c == '\n';
            if (space) {
                // Whitespace collapses, and never opens a paragraph.
                if (!out.text.empty() && out.text.back() != '\n') pendingSpace = true;
                continue;
            }
            // A soft hyphen would sit inside the word when it is looked up.
            if (c == '\xC2' && i + 1 < chunk.size() && chunk[i + 1] == '\xAD') { ++i; continue; }
            if (pendingSpace) out.text += ' ';
            pendingSpace = false;
            out.text += c;
        }
    }
};

}  // namespace

PlainText plain(const std::string& markup) {
    Builder builder;
    XmlScanner::Handlers handlers;
    handlers.onStart = [&](const std::string& name, const XmlScanner::Attributes& attributes) { builder.start(name, attributes); };
    handlers.onEnd = [&](const std::string& name) { builder.end(name); };
    handlers.onText = [&](const std::string& text) { builder.text(text); };
    XmlScanner::scan(markup, handlers);
    while (!builder.openSpans.empty()) builder.closeSpan(builder.openSpans.back().first);
    while (!builder.out.text.empty() && builder.out.text.back() == '\n') builder.out.text.pop_back();
    return builder.out;
}

}  // namespace HtmlText
