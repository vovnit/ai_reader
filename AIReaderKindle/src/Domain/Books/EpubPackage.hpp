#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

/// The parsed contents of an EPUB package document (the `.opf` file): what the
/// book is called, which files it is made of, and in what order they are read.
struct EpubPackage {
    struct Item {
        std::string id;
        std::string href;
        std::string mediaType;
        std::string properties;
    };

    std::string title;
    std::string author;
    std::string language;
    std::map<std::string, Item> items;
    std::vector<std::string> spine;
    std::string coverItemId;
    /// The NCX the EPUB 2 spine points at, by `<spine toc="…">`.
    std::string ncxItemId;

    /// Items to render, in reading order, limited to markup documents.
    std::vector<Item> readingOrder() const;

    /// The manifest item holding the cover image, either flagged by the EPUB 3
    /// `cover-image` property or pointed at by the EPUB 2 `cover` meta tag.
    std::optional<Item> coverItem() const;

    /// The manifest item holding the table of contents: the EPUB 3
    /// navigation document (property `nav`) or, failing that, the NCX.
    std::optional<Item> navigationItem() const;

    static EpubPackage parse(const std::string& xml);

    /// Reads `META-INF/container.xml` to find the package document's path.
    static std::string packagePath(const std::string& containerXml);

    /// Resolves an href relative to the package document into a path relative
    /// to the archive root.
    static std::string resolve(const std::string& href, const std::string& directory);
};
