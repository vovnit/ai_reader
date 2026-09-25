#include "ChatView.hpp"

#include "../Common/Keyboard.hpp"
#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"
#include "ChatFeature.hpp"

namespace ChatView {

namespace {

struct Screen {
    ChatFeature feature;
    GtkWidget* thread = nullptr;
    GtkWidget* scroller = nullptr;
    GtkWidget* entry = nullptr;
    GtkWidget* send = nullptr;
    std::string hint;

    Screen(Env& env, const Seed& seed) : feature(env, seed.context, seed.scope), hint(seed.hint) {}

    void render() {
        GList* children = gtk_container_get_children(GTK_CONTAINER(thread));
        for (GList* child = children; child; child = child->next) gtk_widget_destroy(GTK_WIDGET(child->data));
        g_list_free(children);

        auto add = [&](GtkWidget* widget) { gtk_box_pack_start(GTK_BOX(thread), widget, FALSE, FALSE, 0); };
        if (feature.turns().empty()) add(Widgets::markup("<i>" + Widgets::escape(hint) + "</i>"));
        for (const auto& turn : feature.turns()) {
            std::string text = Widgets::escape(turn.text);
            add(turn.isReader ? Widgets::markup("<b>" + text + "</b>", 1) : Widgets::label(turn.text));
        }
        if (feature.isAnswering()) add(Widgets::label("…"));
        if (!feature.error().empty()) add(Widgets::markup(Widgets::small(Widgets::escape(feature.error()))));
        gtk_widget_show_all(thread);
        gtk_widget_set_sensitive(send, !feature.isAnswering());

        // Keep the newest turn in view, once the new labels have a size.
        GtkAdjustment* vertical = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroller));
        g_object_ref(vertical);
        g_idle_add([](gpointer data) -> gboolean {
            auto* adjustment = static_cast<GtkAdjustment*>(data);
            gtk_adjustment_set_value(adjustment, gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment));
            g_object_unref(adjustment);
            return FALSE;  // one-shot
        }, vertical);
    }

    void submit() {
        std::string question = Widgets::entryText(entry);
        gtk_entry_set_text(GTK_ENTRY(entry), "");
        feature.send(question);
    }
};

}  // namespace

void open(Env& env, Navigator& navigator, const Seed& seed) {
    auto* screen = new Screen(env, seed);

    screen->thread = gtk_vbox_new(FALSE, Widgets::px(10));
    gtk_container_set_border_width(GTK_CONTAINER(screen->thread), Widgets::px(12));
    screen->scroller = Widgets::scrolled(screen->thread);

    screen->entry = Widgets::entry("");
    Widgets::connect(screen->entry, "activate", [screen] { screen->submit(); });
    screen->send = Widgets::button("Send", [screen] { screen->submit(); });
    GtkWidget* composer = gtk_hbox_new(FALSE, Widgets::px(8));
    gtk_container_set_border_width(GTK_CONTAINER(composer), Widgets::px(8));
    gtk_box_pack_start(GTK_BOX(composer), screen->entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(composer), screen->send, FALSE, FALSE, 0);

    // The question field at the top, the keyboard at the bottom, the thread
    // between them.
    GtkWidget* body = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), composer, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), screen->scroller, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Keyboard::create(body), FALSE, FALSE, 0);

    GtkWidget* widget = Widgets::screen("Chat", body, [&navigator] { navigator.pop(); });
    Widgets::own(widget, screen);
    screen->feature.onChange = [screen] { screen->render(); };
    screen->render();
    navigator.push(widget);
    gtk_widget_grab_focus(screen->entry);
}

}  // namespace ChatView
