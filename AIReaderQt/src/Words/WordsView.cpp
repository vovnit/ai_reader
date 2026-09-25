#include "Words/WordsView.hpp"

#include "Common/Navigator.hpp"
#include "Common/Ui.hpp"
#include "Match/MatchView.hpp"
#include "Services/Paths.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

WordsView::WordsView(Env& env, Navigator& navigator, long long bookId)
    : Screen(navigator, bookId ? "Lookups" : "Words"), env_(env), feature_(env, bookId) {
    auto* holder = new QWidget;
    list_ = Ui::column(holder, 14);
    setBody(Ui::scrolled(holder));
    addAction("Export for Anki", [this] { exportCards(); });
    addAction("Practice", [this, bookId] { this->navigator.push(new MatchView(env_, this->navigator, bookId)); });
    feature_.onChange = [this] { render(); };
    render();
}

void WordsView::render() {
    Ui::clear(list_);
    if (feature_.lookups().empty()) list_->addWidget(Ui::label("No words yet. Words you look up while reading collect here."));
    for (const auto& lookup : feature_.lookups()) {
        list_->addWidget(row(lookup));
        list_->addWidget(Ui::separator());
    }
}

QWidget* WordsView::row(const Lookup& lookup) {
    auto* line = new QWidget;
    auto* layout = new QHBoxLayout(line);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* text = new QVBoxLayout;
    text->setSpacing(3);
    QString head = "<b>" + Ui::escape(lookup.word) + "</b>";
    if (!lookup.lemma.empty() && lookup.lemma != lookup.word) head += "&nbsp;&nbsp;" + Ui::small(Ui::escape(lookup.lemma));
    text->addWidget(Ui::rich(head));
    text->addWidget(Ui::label(lookup.meaning));
    if (!lookup.sentence.empty()) text->addWidget(Ui::note("<i>" + Ui::escape(lookup.sentence) + "</i>"));
    layout->addLayout(text, 1);

    auto* remove = new QPushButton("✕");
    remove->setToolTip("Forget the word");
    QObject::connect(remove, &QPushButton::clicked, this, [this, lookup] { feature_.remove(lookup); });
    layout->addWidget(remove, 0, Qt::AlignTop);
    return line;
}

void WordsView::exportCards() {
    size_t count = feature_.lookups().size();
    if (auto path = feature_.exportToAnki()) {
        Ui::alert(this, std::to_string(count) + (count == 1 ? " card" : " cards") + " written for Anki",
                  "Import " + *path + " in Anki.");
    } else {
        Ui::alert(this, "Couldn’t write the cards", Paths::ankiCards());
    }
}
