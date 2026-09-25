#pragma once

#include "ReadingPlace.hpp"

#include <optional>
#include <string>

/// A book in the library. The EPUB stays where it was found on disk; this row
/// only records what it is called and how far it has been read.
struct Book {
    long long id = 0;
    std::string title;
    std::string author;
    std::string language;
    /// Absolute path of the `.epub` file.
    std::string path;
    std::string addedAt;
    /// Where the reader was last: a spine item and a byte offset into its text.
    int readingChapter = 0;
    int readingOffset = 0;
    /// The group the book was put in, or 0. Books in one group — a series,
    /// a course — are searched together.
    long long groupId = 0;
    /// The same place as the position above, in the form another device
    /// can use; `placePending` when it arrived from one and the offset has
    /// not been worked out from it yet, which the reader does when it opens.
    std::optional<ReadingPlace> place;
    bool placePending = false;
    /// When the position or the group last changed, for syncing; empty for
    /// a book never read here, which any other device's record outranks.
    std::string updatedAt;
    /// Its file in the sync folder's `Books`, once it is there.
    std::string remoteName;
};

/// A set of books read together.
struct BookGroup {
    long long id = 0;
    std::string name;
};

/// A place in a book: a spine item and a byte offset into its text.
struct BookPosition {
    long long bookId = 0;
    int chapter = 0;
    int offset = 0;
};
