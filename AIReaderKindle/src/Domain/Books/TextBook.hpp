#pragma once

#include "BookDraft.hpp"

#include <string>

/// Reads a plain-text book, already decoded. Text wrapped by hand, with
/// blank lines between paragraphs, is joined back into paragraphs; text
/// without blank lines has one paragraph per line. A short paragraph that
/// names a chapter or is only a number opens a chapter. The same as the web
/// app's `TextBook.js` and the iOS app's `TextBook.swift`.
namespace TextBook {

/// `fallbackTitle` names a book that does not name itself.
BookDraft read(const std::string& text, const std::string& fallbackTitle);

/// "CHAPTER I.", "Глава 3", "XIV", "12.": kept simple, since a heading one
/// app read differently would move every chapter after it.
bool opensChapter(const std::string& text);

}  // namespace TextBook
