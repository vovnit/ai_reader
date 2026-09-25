#include "CardStore.hpp"

#include "Migrations.hpp"

#include <map>

std::vector<Card> CardStore::all(long long bookId) {
    std::vector<Card> cards;
    std::map<long long, size_t> position;
    for (const auto& lookup : lookups_.all(bookId)) {
        position[lookup.id] = cards.size();
        cards.push_back(Card::fromLookup(lookup));
    }

    Statement query(database_, "SELECT lookupID, correct, wrong, practicedAt FROM cardPractice");
    while (query.step()) {
        auto found = position.find(query.integer(0));
        if (found == position.end()) continue;
        Card& card = cards[found->second];
        card.correct = static_cast<int>(query.integer(1));
        card.wrong = static_cast<int>(query.integer(2));
        card.practicedAt = query.text(3);
    }
    return cards;
}

void CardStore::record(long long lookupId, bool correct) {
    std::string now = Migrations::now();
    // Two statements rather than an upsert, which the Kindle's SQLite may predate.
    Statement insert(database_, "INSERT OR IGNORE INTO cardPractice (lookupID, practicedAt) VALUES (?, ?)");
    insert.bind(1, lookupId).bind(2, now).run();
    Statement update(database_,
        "UPDATE cardPractice SET correct = correct + ?, wrong = wrong + ?, practicedAt = ? WHERE lookupID = ?");
    update.bind(1, correct ? 1 : 0).bind(2, correct ? 0 : 1).bind(3, now).bind(4, lookupId).run();
}

void CardStore::set(long long lookupId, int correct, int wrong, const std::string& practicedAt) {
    Statement replace(database_,
        "INSERT OR REPLACE INTO cardPractice (lookupID, correct, wrong, practicedAt) VALUES (?, ?, ?, ?)");
    replace.bind(1, lookupId).bind(2, correct).bind(3, wrong).bind(4, practicedAt).run();
}
