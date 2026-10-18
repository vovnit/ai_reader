#pragma once

#include "BookDraft.hpp"

#include <string>

/// Reads a plain-text book, already decoded. Text wrapped by hand, with
/// blank lines between paragraphs and short lines, is joined back into
/// paragraphs; otherwise each line is a paragraph. A short paragraph that
/// is a chapter's number, or names a chapter the way a heading does, opens
/// a chapter. The same as the web app's `TextBook.js` and the iOS app's
/// `TextBook.swift`.
namespace TextBook {

/// `fallbackTitle` names a book that does not name itself.
BookDraft read(const std::string& text, const std::string& fallbackTitle);

/// "XIV", "12.", "CHAPTER I.", "Глава 3", "Prologue", "Глава первая",
/// "I. A SCANDAL IN BOHEMIA": a number alone; a chapter word alone, before a
/// number, in a line without a lower-case letter, or in one that does not
/// end as a sentence does; or a number with a stop or a dash before a line
/// without a lower-case letter. "Part of me wanted to stay." is a sentence.
/// Kept simple, since a heading one app read differently would move every
/// chapter after it.
bool opensChapter(const std::string& text);

}  // namespace TextBook
