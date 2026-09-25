#include "EntryView.hpp"

#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"

namespace EntryView {

void open(Navigator& navigator, const std::string& lemma, const std::vector<DictionaryLookup::Article>& articles) {
    GtkWidget* box = gtk_vbox_new(FALSE, Widgets::px(10));
    gtk_container_set_border_width(GTK_CONTAINER(box), Widgets::px(12));
    auto add = [&](GtkWidget* widget) { gtk_box_pack_start(GTK_BOX(box), widget, FALSE, FALSE, 0); };

    for (size_t i = 0; i < articles.size(); ++i) {
        const auto& article = articles[i];
        if (i > 0) add(Widgets::separator());
        std::string head = "<b>" + Widgets::escape(article.lemma) + "</b>";
        if (!article.partOfSpeech.empty()) head += "  " + Widgets::small(Widgets::escape(article.partOfSpeech));
        add(Widgets::markup(head));
        for (size_t n = 0; n < article.senses.size(); ++n) {
            add(Widgets::label(std::to_string(n + 1) + ".  " + article.senses[n]));
        }
        if (!article.source.empty()) add(Widgets::markup(Widgets::small(Widgets::escape(article.source))));
    }

    navigator.push(Widgets::screen(lemma, Widgets::scrolled(box), [&navigator] { navigator.pop(); }));
}

}  // namespace EntryView
