#include "MatchRound.hpp"

#include <algorithm>
#include <numeric>
#include <random>

namespace {

bool anyRowAligned(const std::vector<int>& fronts, const std::vector<int>& backs) {
    for (size_t i = 0; i < fronts.size(); ++i) if (fronts[i] == backs[i]) return true;
    return false;
}

}  // namespace

MatchRound::MatchRound(std::vector<Card> cards, unsigned seed)
    : cards_(std::move(cards)), matched_(cards_.size(), false) {
    fronts_.resize(cards_.size());
    std::iota(fronts_.begin(), fronts_.end(), 0);
    backs_ = fronts_;
    std::mt19937 random(seed);
    std::shuffle(fronts_.begin(), fronts_.end(), random);
    // A row that lines up would give its answer away.
    do {
        std::shuffle(backs_.begin(), backs_.end(), random);
    } while (cards_.size() > 1 && anyRowAligned(fronts_, backs_));
}

int MatchRound::matchedCount() const {
    return static_cast<int>(std::count(matched_.begin(), matched_.end(), true));
}

bool MatchRound::canPick(int card) const {
    return card >= 0 && card < static_cast<int>(cards_.size()) && !matched_[card];
}

std::optional<MatchRound::Pick> MatchRound::pickFront(int card) {
    lastPick_.reset();
    if (!canPick(card)) return std::nullopt;
    if (front_ == card) front_.reset(); else front_ = card;
    return resolve();
}

std::optional<MatchRound::Pick> MatchRound::pickBack(int card) {
    lastPick_.reset();
    if (!canPick(card)) return std::nullopt;
    if (back_ == card) back_.reset(); else back_ = card;
    return resolve();
}

std::optional<MatchRound::Pick> MatchRound::resolve() {
    if (!front_ || !back_) return std::nullopt;
    Pick pick{*front_, *back_, *front_ == *back_};
    if (pick.matched) matched_[pick.front] = true; else ++misses_;
    front_.reset();
    back_.reset();
    lastPick_ = pick;
    return pick;
}
