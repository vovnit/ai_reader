#pragma once

#include "PdfObject.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

/// A bookmark: a title in the PDF's outline and the page it opens.
struct PdfBookmark {
    std::string title;
    int page = 0;
};

/// A PDF file, read for its text. Its objects are found by scanning the
/// file rather than through the cross-reference table, so a damaged or
/// updated file reads as well as a clean one. Encrypted files are refused.
class PdfDocument {
public:
    struct Page {
        int objectNumber = 0;
        PdfObject dictionary;
        /// Its own resources, or those it inherits.
        PdfObject resources;
        /// The top of its media box, which page coordinates count up from.
        double top = 792;
    };

    /// Throws `std::runtime_error` with a reason fit to show.
    explicit PdfDocument(std::string data);

    /// `value`, or the object it refers to; null for a missing one.
    const PdfObject& resolve(const PdfObject& value) const;
    /// A dictionary's or stream's member, resolved.
    const PdfObject& get(const PdfObject& dictionary, const std::string& key) const;
    /// A stream's bytes with its filters undone; empty for an image's
    /// encoding, which text never needs.
    std::string contents(const PdfObject& stream) const;

    const std::vector<Page>& pages() const { return pages_; }
    /// The top level of the outline, the book's chapters; a lone entry
    /// holding the rest gives way to its children.
    std::vector<PdfBookmark> outline() const;
    /// A document information entry, such as `Title` or `Author`.
    std::string info(const std::string& key) const;
    /// The language the catalog declares, as written there.
    std::string language() const;

private:
    std::string data_;
    std::map<int, PdfObject> objects_;
    PdfObject trailer_;
    PdfObject catalog_;
    std::vector<Page> pages_;
    std::map<int, int> pageIndex_;

    void scan();
    void readObjectStreams(std::map<int, size_t>& definedAt);
    void readTrailer(const std::map<int, size_t>& definedAt);
    void collectPages(const PdfObject& node, const PdfObject& resources, double top, int depth, std::set<int>& seen);
    int destinationPage(const PdfObject& item, const std::map<std::string, PdfObject>& named) const;
    void namedDestinations(const PdfObject& node, std::map<std::string, PdfObject>& named, int depth) const;
};
