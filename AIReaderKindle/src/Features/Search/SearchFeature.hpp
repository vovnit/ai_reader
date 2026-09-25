#pragma once

#include "../../Domain/Search/BookSearch.hpp"
#include "../../Services/BookCorpus.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

/// Finding a phrase in the book being read and the rest of its group. The
/// reader's own search covers the whole text — what to know ahead of time is
/// their choice; only the model is kept to the pages read.
class SearchFeature {
public:
    /// How many hits are listed.
    static constexpr int limit = 100;

    explicit SearchFeature(ReadingScope scope);

    const std::string& query() const { return query_; }
    const std::vector<SearchHit>& hits() const { return hits_; }
    bool isSearching() const { return isSearching_; }
    const std::string& error() const { return error_; }
    bool severalBooks() const { return scope_.corpus && scope_.corpus->severalBooks(); }

    /// Runs a search; an empty query, or one made while searching, is ignored.
    void search(const std::string& query);

    std::function<void()> onChange;

private:
    ReadingScope scope_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    std::string query_;
    std::vector<SearchHit> hits_;
    bool isSearching_ = false;
    std::string error_;
};
