#pragma once

#include "../../Domain/Cards/MatchRound.hpp"
#include "../../Services/Env.hpp"

#include <functional>
#include <optional>

/// The matching game over the words looked up so far: a few cards a round,
/// fronts against backs, and another round once they are all paired. Every
/// pair put together is recorded, so a card that was missed comes round
/// again sooner.
class MatchFeature {
public:
    static constexpr size_t cardsPerRound = 5;

    /// `bookId` narrows the cards to one book; 0 plays with every word met.
    MatchFeature(Env& env, long long bookId);

    /// Nothing to play until two words have been looked up.
    bool hasRound() const { return round_.has_value(); }
    const MatchRound& round() const { return *round_; }
    /// Counts up from 1; a view rebuilds its board when it changes.
    int roundNumber() const { return roundNumber_; }

    void pickFront(int card);
    void pickBack(int card);
    void nextRound();

    std::function<void()> onChange;

private:
    Env& env_;
    long long bookId_;
    std::optional<MatchRound> round_;
    int roundNumber_ = 0;

    void record(const std::optional<MatchRound::Pick>& pick);
};
