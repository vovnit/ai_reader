#pragma once

#include "../../Support/PdfPageText.hpp"

#include <string>
#include <vector>

/// A paragraph of a PDF's text, or a heading, and the page it starts on.
struct PdfParagraph {
    std::string text;
    bool heading = false;
    int page = 0;
};

/// Turns a PDF's lines back into paragraphs, undoing what the page did:
/// running heads and page numbers are dropped, lines are joined — a word
/// hyphenated at a line's end mended — and a paragraph a page break cut is
/// joined again. A line set larger than the text is a heading.
namespace PdfParagraphs {

std::vector<PdfParagraph> read(const std::vector<std::vector<PdfLine>>& pages);

}  // namespace PdfParagraphs
