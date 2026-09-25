#include "SearchView.hpp"

#include "../Common/Keyboard.hpp"
#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"
#include "HitView.hpp"
#include "SearchFeature.hpp"

namespace SearchView {

namespace {

struct Screen {
    SearchFeature feature;
    ReaderLink link;
    std::string covers;
    GtkWidget* list = nullptr;
    GtkWidget* entry = nullptr;
    GtkWidget* go = nullptr;

    Screen(const ReaderLink& link, std::string covers) : feature(link.scope), link(link), covers(std::move(covers)) {}

    void render() {
        GList* children = gtk_container_get_children(GTK_CONTAINER(list));
        for (GList* child = children; child; child = child->next) gtk_widget_destroy(GTK_WIDGET(child->data));
        g_list_free(children);

        auto add = [&](GtkWidget* widget) { gtk_box_pack_start(GTK_BOX(list), widget, FALSE, FALSE, 0); };
        if (feature.isSearching()) {
            add(Widgets::label("Searching…"));
        } else if (feature.query().empty()) {
            add(Widgets::markup("<i>" + Widgets::escape("Searches " + covers + ". Tap a result to go there.") + "</i>"));
        } else {
            size_t count = feature.hits().size();
            std::string caption = count == 0 ? "Nothing found for “" + feature.query() + "”."
                : (count >= static_cast<size_t>(SearchFeature::limit) ? "First " : "")
                    + std::to_string(count) + (count == 1 ? " place" : " places") + " with “" + feature.query() + "”";
            add(Widgets::markup(Widgets::small(Widgets::escape(caption))));
            for (const auto& hit : feature.hits()) {
                add(HitView::create(hit, feature.severalBooks(), link.jump));
                add(Widgets::separator());
            }
        }
        if (!feature.error().empty()) add(Widgets::markup(Widgets::small(Widgets::escape(feature.error()))));
        gtk_widget_show_all(list);
        gtk_widget_set_sensitive(go, !feature.isSearching());
    }

    void submit() { feature.search(Widgets::entryText(entry)); }
};

}  // namespace

void open(Navigator& navigator, const ReaderLink& link, const std::string& covers) {
    auto* screen = new Screen(link, covers);

    screen->list = gtk_vbox_new(FALSE, Widgets::px(10));
    gtk_container_set_border_width(GTK_CONTAINER(screen->list), Widgets::px(12));

    screen->entry = Widgets::entry("");
    Widgets::connect(screen->entry, "activate", [screen] { screen->submit(); });
    screen->go = Widgets::button("Search", [screen] { screen->submit(); });
    GtkWidget* composer = gtk_hbox_new(FALSE, Widgets::px(8));
    gtk_container_set_border_width(GTK_CONTAINER(composer), Widgets::px(8));
    gtk_box_pack_start(GTK_BOX(composer), screen->entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(composer), screen->go, FALSE, FALSE, 0);

    // The query at the top, the keyboard at the bottom, the hits between.
    GtkWidget* body = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), composer, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::scrolled(screen->list), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Keyboard::create(body), FALSE, FALSE, 0);

    GtkWidget* widget = Widgets::screen("Search", body, [&navigator] { navigator.pop(); });
    Widgets::own(widget, screen);
    screen->feature.onChange = [screen] { screen->render(); };
    screen->render();
    navigator.push(widget);
    gtk_widget_grab_focus(screen->entry);
}

}  // namespace SearchView
