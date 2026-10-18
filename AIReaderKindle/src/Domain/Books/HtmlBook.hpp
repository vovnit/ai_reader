#pragma once

#include "BookDraft.hpp"

#include <string>

/// Reads an HTML book, already decoded: each block element's text is a
/// paragraph, a heading's a heading, and `<h1>`, `<h2>` and `<h3>` open
/// chapters (Gutenberg often sets parts in `<h2>` and their chapters in
/// `<h3>`). Tolerant of what real files carry: unclosed paragraphs, stray end tags,
/// upper-case names, no `<html>` or `<body>`. The same as the web app's
/// `HtmlBook.js` and the iOS app's `HTMLBook.swift`.
namespace HtmlBook {

/// `fallbackTitle` names a book that does not name itself.
BookDraft read(const std::string& text, const std::string& fallbackTitle);

}  // namespace HtmlBook
