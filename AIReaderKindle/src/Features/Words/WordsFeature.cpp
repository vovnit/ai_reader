#include "WordsFeature.hpp"

#include "../../Domain/Cards/AnkiExport.hpp"
#include "../../Services/Paths.hpp"
#include "../../Support/Files.hpp"

WordsFeature::WordsFeature(Env& env, long long bookId) : env_(env), bookId_(bookId) {
    lookups_ = env_.lookups.all(bookId_);
}

void WordsFeature::reload() {
    lookups_ = env_.lookups.all(bookId_);
    if (onChange) onChange();
}

void WordsFeature::remove(const Lookup& lookup) {
    env_.lookups.remove(lookup.id);
    reload();
}

std::optional<std::string> WordsFeature::exportToAnki() {
    std::vector<Card> cards;
    for (const auto& lookup : lookups_) cards.push_back(Card::fromLookup(lookup));
    // One book's words go to a deck of their own under the app's.
    std::string deck = "AIReader";
    if (auto book = bookId_ ? env_.library.find(bookId_) : std::nullopt) deck += "::" + book->title;
    std::string path = Paths::ankiCards();
    if (!Files::write(path, AnkiExport::text(cards, deck))) return std::nullopt;
    return path;
}
