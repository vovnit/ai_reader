#pragma once

#include "../Domain/Cards/Card.hpp"
#include "Database.hpp"
#include "LookupCache.hpp"

#include <vector>

/// Every lookup as a flash card. The cards themselves are the lookups; only
/// how each has fared in practice is kept here, and it goes when the lookup
/// does.
class CardStore {
public:
    CardStore(Database& database, LookupCache& lookups) : database_(database), lookups_(lookups) {}

    /// Every card, newest lookup first; `bookId` narrows it to one book.
    std::vector<Card> all(long long bookId = 0);
    /// Counts one practice outcome against the card.
    void record(long long lookupId, bool correct);
    /// Sets the record outright, as another device reports it.
    void set(long long lookupId, int correct, int wrong, const std::string& practicedAt);

private:
    Database& database_;
    LookupCache& lookups_;
};
