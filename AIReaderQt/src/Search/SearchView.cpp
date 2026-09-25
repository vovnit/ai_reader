#include "Search/SearchView.hpp"

#include "Common/Ui.hpp"
#include "Search/HitView.hpp"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

SearchView::SearchView(Navigator& navigator, const ReaderLink& link, std::string covers)
    : Screen(navigator, "Search"), feature_(link.scope), link_(link), covers_(std::move(covers)) {
    auto* body = new QWidget;
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* composer = new QHBoxLayout;
    composer->setContentsMargins(12, 8, 12, 8);
    query_ = new QLineEdit;
    query_->setPlaceholderText("A word or a phrase");
    go_ = new QPushButton("Search");
    auto submit = [this] { feature_.search(Ui::s(query_->text())); };
    QObject::connect(query_, &QLineEdit::returnPressed, this, submit);
    QObject::connect(go_, &QPushButton::clicked, this, submit);
    composer->addWidget(query_, 1);
    composer->addWidget(go_);
    layout->addLayout(composer);
    layout->addWidget(Ui::separator());

    auto* holder = new QWidget;
    list_ = Ui::column(holder, 10);
    layout->addWidget(Ui::scrolled(holder), 1);
    setBody(body);
    setFocusProxy(query_);

    feature_.onChange = [this] { render(); };
    render();
}

void SearchView::render() {
    Ui::clear(list_);
    if (feature_.isSearching()) {
        list_->addWidget(Ui::label("Searching…"));
    } else if (feature_.query().empty()) {
        list_->addWidget(Ui::rich("<i>" + Ui::escape("Searches " + covers_ + ". Click a result to go there.") + "</i>"));
    } else {
        size_t count = feature_.hits().size();
        std::string caption = count == 0 ? "Nothing found for “" + feature_.query() + "”."
            : (count >= static_cast<size_t>(SearchFeature::limit) ? "First " : "")
                + std::to_string(count) + (count == 1 ? " place" : " places") + " with “" + feature_.query() + "”";
        list_->addWidget(Ui::note(Ui::escape(caption)));
        for (const auto& hit : feature_.hits()) {
            list_->addWidget(HitView::create(hit, feature_.severalBooks(), link_.jump));
            list_->addWidget(Ui::separator());
        }
    }
    if (!feature_.error().empty()) list_->addWidget(Ui::note(Ui::escape(feature_.error())));
    go_->setEnabled(!feature_.isSearching());
}
