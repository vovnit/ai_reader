#pragma once

#include "Card.hpp"

#include <optional>
#include <vector>

/// One round of the matching game: a few cards, their fronts in one column
/// and their backs in another, shuffled apart. The reader taps a front and a
/// back; a pair that belongs together is put aside, one that does not is a
/// miss. Pure state, so it can be checked without a screen.
class MatchRound {
public:
    /// A front and a back the reader put together, as indices into `cards()`.
    struct Pick {
        int front;
        int back;
        bool matched;
    };

    MatchRound(std::vector<Card> cards, unsigned seed);

    const std::vector<Card>& cards() const { return cards_; }
    /// The order the fronts and backs are shown in, as indices into `cards()`.
    /// No back sits in the same row as its front.
    const std::vector<int>& fronts() const { return fronts_; }
    const std::vector<int>& backs() const { return backs_; }
    bool isMatched(int card) const { return matched_[card]; }
    std::optional<int> selectedFront() const { return front_; }
    std::optional<int> selectedBack() const { return back_; }
    /// The pair most recently put together, until the next tap.
    const std::optional<Pick>& lastPick() const { return lastPick_; }
    int matchedCount() const;
    int misses() const { return misses_; }
    bool isComplete() const { return matchedCount() == static_cast<int>(cards_.size()); }

    /// A tap on a front or a back. Tapping the chosen one again lets go of
    /// it; once one of each is chosen the pair resolves and is returned.
    std::optional<Pick> pickFront(int card);
    std::optional<Pick> pickBack(int card);

private:
    std::vector<Card> cards_;
    std::vector<int> fronts_;
    std::vector<int> backs_;
    std::vector<bool> matched_;
    std::optional<int> front_;
    std::optional<int> back_;
    std::optional<Pick> lastPick_;
    int misses_ = 0;

    bool canPick(int card) const;
    std::optional<Pick> resolve();
};
