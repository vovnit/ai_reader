#include "EpubPackage.hpp"

#include "../../Support/Text.hpp"
#include "../../Support/XmlScanner.hpp"

#include <glib.h>

std::vector<EpubPackage::Item> EpubPackage::readingOrder() const {
    std::vector<Item> order;
    for (const auto& id : spine) {
        auto item = items.find(id);
        if (item != items.end() && Text::contains(item->second.mediaType, "html")) {
            order.push_back(item->second);
        }
    }
    return order;
}

std::optional<EpubPackage::Item> EpubPackage::coverItem() const {
    for (const auto& entry : items) {
        if (Text::contains(entry.second.properties, "cover-image")) return entry.second;
    }
    auto flagged = items.find(coverItemId);
    if (flagged != items.end()) return flagged->second;
    return std::nullopt;
}

std::optional<EpubPackage::Item> EpubPackage::navigationItem() const {
    for (const auto& entry : items) {
        for (const auto& property : Text::split(entry.second.properties, ' ')) {
            if (property == "nav") return entry.second;
        }
    }
    auto named = items.find(ncxItemId);
    if (named != items.end()) return named->second;
    for (const auto& entry : items) {
        if (entry.second.mediaType == "application/x-dtbncx+xml") return entry.second;
    }
    return std::nullopt;
}

static std::string unescapeHref(const std::string& href) {
    gchar* unescaped = g_uri_unescape_string(href.c_str(), nullptr);
    std::string result = unescaped ? unescaped : href;
    g_free(unescaped);
    return result;
}

EpubPackage EpubPackage::parse(const std::string& xml) {
    EpubPackage package;
    bool inSpine = false;
    std::string open;
    std::string text;

    XmlScanner::Handlers handlers;
    handlers.onStart = [&](const std::string& name, const XmlScanner::Attributes& attributes) {
        open = name;
        text.clear();
        auto attribute = [&](const char* key) {
            auto found = attributes.find(key);
            return found == attributes.end() ? std::string() : found->second;
        };
        if (name == "item") {
            std::string id = attribute("id");
            std::string href = attribute("href");
            if (id.empty() || href.empty()) return;
            package.items[id] = Item{id, unescapeHref(href), attribute("media-type"), attribute("properties")};
        } else if (name == "spine") {
            inSpine = true;
            package.ncxItemId = attribute("toc");
        } else if (name == "itemref") {
            if (inSpine && !attribute("idref").empty()) package.spine.push_back(attribute("idref"));
        } else if (name == "meta") {
            if (attribute("name") == "cover") package.coverItemId = attribute("content");
        }
    };
    handlers.onText = [&](const std::string& chunk) { text += chunk; };
    handlers.onEnd = [&](const std::string& name) {
        std::string value = Text::trim(text);
        if (name == open && !value.empty()) {
            if (name == "title" && package.title.empty()) package.title = value;
            else if (name == "creator" && package.author.empty()) package.author = value;
            else if (name == "language" && package.language.empty()) package.language = value;
        }
        text.clear();
    };
    XmlScanner::scan(xml, handlers);
    return package;
}

std::string EpubPackage::packagePath(const std::string& containerXml) {
    std::string path;
    XmlScanner::Handlers handlers;
    handlers.onStart = [&](const std::string& name, const XmlScanner::Attributes& attributes) {
        if (name == "rootfile" && path.empty()) {
            auto full = attributes.find("full-path");
            if (full != attributes.end()) path = full->second;
        }
    };
    XmlScanner::scan(containerXml, handlers);
    return path;
}

std::string EpubPackage::resolve(const std::string& href, const std::string& directory) {
    std::string combined = directory.empty() ? href : directory + "/" + href;
    std::vector<std::string> components;
    for (const auto& part : Text::split(combined, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (!components.empty()) components.pop_back();
            continue;
        }
        components.push_back(part);
    }
    return Text::join(components, "/");
}
