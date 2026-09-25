#pragma once

#include "../../Domain/Books/Book.hpp"
#include "../../Services/BookCorpus.hpp"

#include <functional>

/// What a screen opened from the reader carries: the books to search, how
/// far they have been read, and a way back to a place in them.
struct ReaderLink {
    ReadingScope scope;
    /// Turns the reader to that place and returns to it; empty when the
    /// screen was not opened from a book.
    std::function<void(const BookPosition&)> jump;
};
