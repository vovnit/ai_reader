#pragma once

#include "Common/Screen.hpp"
#include "Features/Match/MatchFeature.hpp"

#include <vector>

class QHBoxLayout;
class QLabel;
class QPushButton;
class Tappable;

/// The matching game: the words of a round on the left, what they meant on
/// the right, paired by clicking one and then the other.
class MatchView : public Screen {
public:
    MatchView(Env& env, Navigator& navigator, long long bookId);

private:
    MatchFeature feature_;
    QLabel* status_;
    QHBoxLayout* columns_;
    QPushButton* next_;
    std::vector<Tappable*> fronts_;
    std::vector<Tappable*> backs_;
    int round_ = 0;

    void render();
    /// The board is laid out once a round; a click only changes which cards
    /// are picked and which are put aside.
    void build();
    void refresh();
};
