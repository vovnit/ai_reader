#include "EpubNavigation.hpp"

#include "../../Support/Text.hpp"
#include "../../Support/XmlScanner.hpp"

namespace EpubNavigation {

namespace {

std::string attribute(const XmlScanner::Attributes& attributes, const char* key) {
    auto found = attributes.find(key);
    return found == attributes.end() ? std::string() : found->second;
}

/// Entries with a title and somewhere to go; titles on one line.
std::vector<NavEntry> tidy(std::vector<NavEntry> entries) {
    std::vector<NavEntry> kept;
    for (auto& entry : entries) {
        entry.title = Text::trim(entry.title);
        if (entry.title.empty() || entry.href.empty()) continue;
        kept.push_back(std::move(entry));
    }
    return kept;
}

}  // namespace

std::vector<NavEntry> fromNcx(const std::string& xml) {
    std::vector<NavEntry> entries;
    // The navPoints open at this point, as indexes into `entries`, so a
    // label or content lands on the innermost one.
    std::vector<size_t> open;
    bool inLabel = false;

    XmlScanner::Handlers handlers;
    handlers.onStart = [&](const std::string& name, const XmlScanner::Attributes& attributes) {
        if (name == "navpoint") {
            entries.push_back({"", "", static_cast<int>(open.size())});
            open.push_back(entries.size() - 1);
        } else if (name == "navlabel") {
            inLabel = !open.empty();
        } else if (name == "content" && !open.empty()) {
            entries[open.back()].href = attribute(attributes, "src");
        }
    };
    handlers.onText = [&](const std::string& text) {
        if (inLabel) entries[open.back()].title += text;
    };
    handlers.onEnd = [&](const std::string& name) {
        if (name == "navpoint" && !open.empty()) open.pop_back();
        else if (name == "navlabel") inLabel = false;
    };
    XmlScanner::scan(xml, handlers);
    return tidy(std::move(entries));
}

std::vector<NavEntry> fromNav(const std::string& xhtml) {
    // Each <nav> is read on its own; the one typed "toc" is the table of
    // contents, and failing that the first one is taken.
    std::vector<std::vector<NavEntry>> navs;
    std::vector<bool> isToc;
    int listDepth = 0;
    bool inLink = false;

    XmlScanner::Handlers handlers;
    handlers.onStart = [&](const std::string& name, const XmlScanner::Attributes& attributes) {
        if (name == "nav") {
            navs.emplace_back();
            isToc.push_back(Text::contains(attribute(attributes, "type"), "toc"));
            listDepth = 0;
            return;
        }
        if (navs.empty()) return;
        if (name == "ol" || name == "ul") ++listDepth;
        else if (name == "li") navs.back().push_back({"", "", std::max(listDepth - 1, 0)});
        else if (name == "a" && !navs.back().empty()) {
            navs.back().back().href = attribute(attributes, "href");
            inLink = true;
        }
    };
    handlers.onText = [&](const std::string& text) {
        if (inLink) navs.back().back().title += text;
    };
    handlers.onEnd = [&](const std::string& name) {
        if (navs.empty()) return;
        if (name == "a") inLink = false;
        else if ((name == "ol" || name == "ul") && listDepth) --listDepth;
    };
    XmlScanner::scan(xhtml, handlers);

    for (size_t i = 0; i < navs.size(); ++i) {
        if (isToc[i]) return tidy(std::move(navs[i]));
    }
    return navs.empty() ? std::vector<NavEntry>{} : tidy(std::move(navs[0]));
}

std::vector<NavEntry> parse(const std::string& markup) {
    bool ncx = false;
    XmlScanner::Handlers handlers;
    handlers.onStart = [&](const std::string& name, const XmlScanner::Attributes&) {
        if (name == "ncx") ncx = true;
    };
    XmlScanner::scan(markup, handlers);
    return ncx ? fromNcx(markup) : fromNav(markup);
}

}  // namespace EpubNavigation
