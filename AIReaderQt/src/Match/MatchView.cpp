#include "Match/MatchView.hpp"

#include "Common/Tappable.hpp"
#include "Common/Ui.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

Tappable* card(const QString& html, std::function<void()> pick) {
    auto* card = new Tappable(std::move(pick));
    card->setProperty("card", true);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->addWidget(Ui::rich(html));
    return card;
}

std::string status(const MatchRound& round) {
    std::string misses = round.misses() == 0 ? "no misses"
        : std::to_string(round.misses()) + (round.misses() == 1 ? " miss" : " misses");
    if (round.isComplete()) return "All " + std::to_string(round.matchedCount()) + " paired, " + misses + ".";
    std::string progress = std::to_string(round.matchedCount()) + " of " + std::to_string(round.cards().size()) + " paired, " + misses + ".";
    if (!round.lastPick()) return round.matchedCount() == 0 ? "Click a word, then the meaning it had." : progress;
    return (round.lastPick()->matched ? "✓ A pair. " : "✕ Not a pair. ") + progress;
}

}  // namespace

MatchView::MatchView(Env& env, Navigator& navigator, long long bookId)
    : Screen(navigator, "Practice"), feature_(env, bookId) {
    auto* holder = new QWidget;
    QVBoxLayout* column = Ui::column(holder);
    status_ = new QLabel;
    columns_ = new QHBoxLayout;
    columns_->setSpacing(10);
    next_ = new QPushButton("Next round");
    QObject::connect(next_, &QPushButton::clicked, this, [this] { feature_.nextRound(); });
    column->addWidget(status_);
    column->addLayout(columns_);
    column->addWidget(next_, 0, Qt::AlignLeft);
    setBody(Ui::scrolled(holder));

    feature_.onChange = [this] { render(); };
    render();
}

void MatchView::render() {
    if (round_ != feature_.roundNumber()) build();
    refresh();
}

void MatchView::build() {
    Ui::clear(columns_);
    fronts_.clear();
    backs_.clear();
    round_ = feature_.roundNumber();
    if (!feature_.hasRound()) {
        columns_->addWidget(Ui::label("Look up two words or more while reading, then come back to pair them with their meanings."));
        return;
    }
    const MatchRound& round = feature_.round();
    auto* fronts = new QVBoxLayout;
    auto* backs = new QVBoxLayout;
    for (int index : round.fronts()) {
        const Card& front = round.cards()[index];
        QString html = "<b>" + Ui::escape(front.front) + "</b>";
        if (!front.lemma.empty()) html += "<br>" + Ui::small(Ui::escape(front.lemma));
        fronts_.push_back(card(html, [this, index] { feature_.pickFront(index); }));
        fronts->addWidget(fronts_.back());
    }
    for (int index : round.backs()) {
        const Card& back = round.cards()[index];
        QString html = Ui::escape(back.back);
        if (!back.example.empty()) html += "<br>" + Ui::small("<i>" + Ui::escape(back.example) + "</i>");
        backs_.push_back(card(html, [this, index] { feature_.pickBack(index); }));
        backs->addWidget(backs_.back());
    }
    columns_->addLayout(fronts, 2);
    columns_->addLayout(backs, 3);
}

void MatchView::refresh() {
    status_->setVisible(feature_.hasRound());
    next_->setVisible(feature_.hasRound() && feature_.round().isComplete());
    if (!feature_.hasRound()) return;
    const MatchRound& round = feature_.round();
    for (size_t i = 0; i < fronts_.size(); ++i) {
        int index = round.fronts()[i];
        fronts_[i]->setSelected(round.selectedFront() == index);
        fronts_[i]->setEnabled(!round.isMatched(index));
    }
    for (size_t i = 0; i < backs_.size(); ++i) {
        int index = round.backs()[i];
        backs_[i]->setSelected(round.selectedBack() == index);
        backs_[i]->setEnabled(!round.isMatched(index));
    }
    status_->setText(Ui::q(status(round)));
}
