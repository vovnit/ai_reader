#include "BookCorpus.hpp"

#include "EpubLoader.hpp"

BookCorpus::BookCorpus(std::vector<Book> books) : books_(std::move(books)) {}

void BookCorpus::provide(long long bookId, std::vector<std::string> chapters) {
    std::lock_guard<std::mutex> lock(mutex_);
    chapters_[bookId] = std::move(chapters);
    errors_.erase(bookId);
}

void BookCorpus::loadMissing() {
    for (const auto& book : books_) {
        if (chapters_.count(book.id) || errors_.count(book.id)) continue;
        std::string error;
        auto document = EpubLoader::load(book.path, &error, false);
        if (!document) {
            errors_[book.id] = book.title + ": " + error;
            continue;
        }
        std::vector<std::string> chapters;
        for (auto& chapter : document->chapters) chapters.push_back(std::move(chapter.text));
        chapters_[book.id] = std::move(chapters);
    }
}

std::vector<SearchHit> BookCorpus::search(const std::string& query, int limit, const std::optional<BookPosition>& upTo) {
    std::lock_guard<std::mutex> lock(mutex_);
    loadMissing();
    std::vector<SearchHit> hits;
    for (const auto& book : books_) {
        auto loaded = chapters_.find(book.id);
        if (loaded == chapters_.end()) continue;
        bool bounded = upTo && upTo->bookId == book.id;
        const std::vector<std::string>& chapters = loaded->second;
        for (size_t chapter = 0; chapter < chapters.size() && static_cast<int>(hits.size()) < limit; ++chapter) {
            if (bounded && static_cast<int>(chapter) > upTo->chapter) break;
            for (SearchHit& hit : BookSearch::find(chapters[chapter], query, static_cast<int>(chapter), limit - static_cast<int>(hits.size()))) {
                if (bounded && static_cast<int>(chapter) == upTo->chapter && hit.offset >= upTo->offset) break;
                hit.bookId = book.id;
                hit.bookTitle = book.title;
                hits.push_back(std::move(hit));
            }
        }
    }
    return hits;
}

std::vector<std::string> BookCorpus::errors() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> list;
    for (const auto& entry : errors_) list.push_back(entry.second);
    return list;
}
