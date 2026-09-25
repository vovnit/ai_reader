#include "WordsView.hpp"

#include "../../Services/Paths.hpp"
#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"
#include "../Match/MatchView.hpp"
#include "WordsFeature.hpp"

namespace WordsView {

namespace {

GtkWidget* row(WordsFeature& feature, const Lookup& lookup) {
    GtkWidget* text = gtk_vbox_new(FALSE, Widgets::px(3));
    std::string head = "<b>" + Widgets::escape(lookup.word) + "</b>";
    if (!lookup.lemma.empty() && lookup.lemma != lookup.word) {
        head += "  " + Widgets::small(Widgets::escape(lookup.lemma));
    }
    gtk_box_pack_start(GTK_BOX(text), Widgets::markup(head), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(text), Widgets::label(lookup.meaning), FALSE, FALSE, 0);
    if (!lookup.sentence.empty()) {
        gtk_box_pack_start(GTK_BOX(text), Widgets::markup(Widgets::small("<i>" + Widgets::escape(lookup.sentence) + "</i>")), FALSE, FALSE, 0);
    }

    GtkWidget* line = gtk_hbox_new(FALSE, Widgets::px(8));
    gtk_box_pack_start(GTK_BOX(line), text, TRUE, TRUE, 0);
    // Deferred: the row, button included, is rebuilt when the list changes.
    GtkWidget* remove = Widgets::glyphButton("✕", [&feature, lookup] {
        Widgets::later([&feature, lookup] { feature.remove(lookup); });
    }, true);
    GtkWidget* holder = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(holder), remove, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(line), holder, FALSE, FALSE, 0);
    return line;
}

}  // namespace

void open(Env& env, Navigator& navigator, long long bookId) {
    auto* feature = new WordsFeature(env, bookId);
    GtkWidget* list = gtk_vbox_new(FALSE, Widgets::px(14));
    gtk_container_set_border_width(GTK_CONTAINER(list), Widgets::px(12));

    auto render = [feature, list] {
        GList* children = gtk_container_get_children(GTK_CONTAINER(list));
        for (GList* child = children; child; child = child->next) gtk_widget_destroy(GTK_WIDGET(child->data));
        g_list_free(children);
        if (feature->lookups().empty()) {
            gtk_box_pack_start(GTK_BOX(list), Widgets::label("No words yet. Words you look up while reading collect here."), FALSE, FALSE, 0);
        }
        for (const auto& lookup : feature->lookups()) {
            gtk_box_pack_start(GTK_BOX(list), row(*feature, lookup), FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(list), Widgets::separator(), FALSE, FALSE, 0);
        }
        gtk_widget_show_all(list);
    };
    feature->onChange = render;
    render();

    std::vector<GtkWidget*> actions = {
        Widgets::button("Export", [&navigator, feature] {
            size_t count = feature->lookups().size();
            if (auto path = feature->exportToAnki()) {
                Widgets::alert(navigator.window(), std::to_string(count) + (count == 1 ? " card" : " cards") + " written for Anki",
                               "Copy " + *path + " to a computer and import it in Anki.");
            } else {
                Widgets::alert(navigator.window(), "Couldn’t write the cards", Paths::ankiCards());
            }
        }),
        Widgets::button("Practice", [&env, &navigator, bookId] { MatchView::open(env, navigator, bookId); }),
    };
    GtkWidget* screen = Widgets::screen(bookId ? "Lookups" : "Words", Widgets::scrolled(list), [&navigator] { navigator.pop(); }, "Back", actions);
    Widgets::own(screen, feature);
    navigator.push(screen);
}

}  // namespace WordsView
