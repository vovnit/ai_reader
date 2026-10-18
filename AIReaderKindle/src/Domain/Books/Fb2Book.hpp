#pragma once

#include "BookDraft.hpp"

#include <string>

/// Reads a FictionBook 2 file, the format most Russian e-books come in: the
/// title, first author and language from its <title-info>, a chapter from
/// each section of its text, the first <body> without a name, and one last
/// chapter from each other body (notes, comments). Books are matched across
/// devices by title and author and reading places by chapter, so this is
/// the same, word for word, as the web app's `Fb2Book.js` and the iOS app's
/// `FB2Book`.
namespace Fb2Book {

/// The book in `text`, already decoded. `fallbackTitle` names a book whose
/// description gives no title.
BookDraft read(const std::string& text, const std::string& fallbackTitle);

}  // namespace Fb2Book
