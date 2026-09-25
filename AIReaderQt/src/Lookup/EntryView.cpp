#include "Lookup/EntryView.hpp"

#include "Common/Ui.hpp"

#include <QVBoxLayout>

EntryView::EntryView(Navigator& navigator, const std::string& lemma, const std::vector<DictionaryLookup::Article>& articles)
    : Screen(navigator, Ui::q(lemma)) {
    auto* holder = new QWidget;
    QVBoxLayout* column = Ui::column(holder, 10);
    for (size_t i = 0; i < articles.size(); ++i) {
        const auto& article = articles[i];
        if (i > 0) column->addWidget(Ui::separator());
        QString head = "<b>" + Ui::escape(article.lemma) + "</b>";
        if (!article.partOfSpeech.empty()) head += "&nbsp;&nbsp;" + Ui::small(Ui::escape(article.partOfSpeech));
        column->addWidget(Ui::rich(head));
        for (size_t n = 0; n < article.senses.size(); ++n) {
            column->addWidget(Ui::label(std::to_string(n + 1) + ".  " + article.senses[n]));
        }
        if (!article.source.empty()) column->addWidget(Ui::note(Ui::escape(article.source)));
    }
    setBody(Ui::scrolled(holder));
}
