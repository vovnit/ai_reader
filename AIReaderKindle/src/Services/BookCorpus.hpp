#pragma once

#include "../Domain/Books/Book.hpp"
#include "../Domain/Search/BookSearch.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

/// The text of the book being read and of the other books in its group,
/// read from disk once, when first searched, and searched from any thread.
class BookCorpus {
public:
    /// In the order they are to be searched: the open book first.
    explicit BookCorpus(std::vector<Book> books);

    const std::vector<Book>& books() const { return books_; }
    bool severalBooks() const { return books_.size() > 1; }

    /// Hands over chapters the reader has already loaded, so that book is
    /// not read again.
    void provide(long long bookId, std::vector<std::string> chapters);

    /// Hits book by book, in reading order, at most `limit`. With `upTo`,
    /// that book is searched only as far as that point — what the reader has
    /// seen — and the others in full.
    std::vector<SearchHit> search(const std::string& query, int limit, const std::optional<BookPosition>& upTo = std::nullopt);

    /// One chapter's text, or nothing when the book cannot be read.
    std::string chapterText(long long bookId, int chapter);

    /// Why a book could not be read, if one could not; checked after a search.
    std::vector<std::string> errors();

private:
    std::mutex mutex_;
    std::vector<Book> books_;
    std::map<long long, std::vector<std::string>> chapters_;
    std::map<long long, std::string> errors_;

    void loadMissing();
    void load(const Book& book);
};

/// What is open in front of the reader, as a lookup, an X-ray or a
/// conversation sees it: the books to search, how far they have been read,
/// and the passage in front of the reader — the sentence of a lookup, the
/// page of a chat — which the model may read around.
struct ReadingScope {
    std::shared_ptr<BookCorpus> corpus;
    std::optional<BookPosition> upTo;
    std::optional<BookPassage> passage;
};
