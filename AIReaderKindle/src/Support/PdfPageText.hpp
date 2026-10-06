#pragma once

#include "PdfDocument.hpp"
#include "PdfFont.hpp"

#include <map>
#include <string>
#include <vector>

/// One line of a page's text as it is laid out, in points: where it starts
/// and ends, how far below the top of the page its baseline is, and how
/// large its letters are.
struct PdfLine {
    std::string text;
    double left = 0;
    double right = 0;
    double y = 0;
    double size = 0;
};

/// Reads a page's content for its text: runs the text operators, places
/// every glyph, and gathers the glyphs into lines, in the order the page
/// draws them. Pictures and paths are passed over.
namespace PdfPageText {

/// The fonts read so far, by their dictionary, so a book's pages read
/// each font once.
using FontCache = std::map<const PdfObject*, PdfFont>;

std::vector<PdfLine> lines(const PdfDocument& document, const PdfDocument::Page& page, FontCache& fonts);

}  // namespace PdfPageText
