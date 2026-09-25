#include "SearchFeature.hpp"

#include "../../Support/Async.hpp"
#include "../../Support/Text.hpp"

SearchFeature::SearchFeature(ReadingScope scope) : scope_(std::move(scope)) {}

void SearchFeature::search(const std::string& query) {
    std::string trimmed = Text::trim(query);
    if (trimmed.empty() || isSearching_ || !scope_.corpus) return;
    query_ = trimmed;
    isSearching_ = true;
    error_.clear();
    if (onChange) onChange();

    struct Found {
        std::vector<SearchHit> hits;
        std::string error;
    };
    std::shared_ptr<BookCorpus> corpus = scope_.corpus;
    Async::run<Found>(
        [corpus, trimmed] {
            Found found;
            found.hits = corpus->search(trimmed, limit);
            found.error = Text::join(corpus->errors(), "\n");
            return found;
        },
        [this](Found found) {
            isSearching_ = false;
            hits_ = std::move(found.hits);
            error_ = std::move(found.error);
            if (onChange) onChange();
        },
        alive_);
}
