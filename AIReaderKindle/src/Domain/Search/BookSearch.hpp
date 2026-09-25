#pragma once

#include <string>
#include <vector>

/// One place a search query occurs.
struct SearchHit {
    long long bookId = 0;
    std::string bookTitle;
    /// Spine item and byte offset of the match in its text.
    int chapter = 0;
    int offset = 0;
    /// The sentence around the match, cut down when it runs long.
    std::string excerpt;
    /// Byte range of the match inside `excerpt`, for highlighting.
    int matchStart = 0;
    int matchEnd = 0;
};

/// Finds a query in a book's text, case-insensitively, and cuts a readable
/// excerpt around every hit. Pure: no display, no disk.
namespace BookSearch {

/// How far an excerpt reaches on either side of its match, in bytes, when
/// the sentence runs longer.
constexpr int reach = 220;

/// Hits in one chapter, in order, at most `limit` of them. A sentence with
/// the query in it twice is one hit.
std::vector<SearchHit> find(const std::string& text, const std::string& query, int chapter, int limit);

/// The excerpt for a match at `[start, end)`: its sentence, or as much of it
/// as fits within `reach` on either side. Sets `matchStart`/`matchEnd`.
SearchHit excerpt(const std::string& text, int start, int end, int reach = BookSearch::reach);

}  // namespace BookSearch
