#include "MatchFeature.hpp"

#include <glib.h>

MatchFeature::MatchFeature(Env& env, long long bookId) : env_(env), bookId_(bookId) {
    nextRound();
}

void MatchFeature::nextRound() {
    std::vector<Card> due = Card::due(env_.cards.all(bookId_), cardsPerRound);
    if (due.size() < 2) round_.reset();
    else round_.emplace(std::move(due), g_random_int());
    ++roundNumber_;
    if (onChange) onChange();
}

void MatchFeature::pickFront(int card) {
    if (!round_) return;
    record(round_->pickFront(card));
    if (onChange) onChange();
}

void MatchFeature::pickBack(int card) {
    if (!round_) return;
    record(round_->pickBack(card));
    if (onChange) onChange();
}

void MatchFeature::record(const std::optional<MatchRound::Pick>& pick) {
    if (!pick) return;
    const std::vector<Card>& cards = round_->cards();
    if (pick->matched) {
        env_.cards.record(cards[pick->front].lookupId, true);
        return;
    }
    // The two were confused with each other; both need another look.
    env_.cards.record(cards[pick->front].lookupId, false);
    env_.cards.record(cards[pick->back].lookupId, false);
}
