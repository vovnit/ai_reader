#include "XRayView.hpp"

#include "../../Domain/AI/ChatPrompt.hpp"
#include "../Chat/ChatView.hpp"
#include "../Common/Keyboard.hpp"
#include "../Common/Navigator.hpp"
#include "../../Support/Text.hpp"
#include "../Common/Widgets.hpp"
#include "../Search/HitView.hpp"
#include "XRayFeature.hpp"

namespace XRayView {

namespace {

GtkWidget* rendered(Env& env, Navigator& navigator, const XRayFeature& feature, const ReaderLink& link) {
    GtkWidget* box = gtk_vbox_new(FALSE, Widgets::px(14));
    gtk_container_set_border_width(GTK_CONTAINER(box), Widgets::px(12));
    auto add = [&](GtkWidget* widget) { gtk_box_pack_start(GTK_BOX(box), widget, FALSE, FALSE, 0); };

    if (feature.isWorking()) {
        add(Widgets::label("Reading the book…"));
    } else if (!feature.answer().empty()) {
        add(Widgets::markup(Widgets::big(Widgets::escape(feature.answer()))));
        std::string seed = ChatPrompt::xrayContext(feature.term(), feature.answer());
        ReadingScope scope = feature.scope();
        GtkWidget* line = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(line), Widgets::button("Ask AI", [&env, &navigator, seed, scope] {
            ChatView::open(env, navigator, {seed, "Ask about this — who they are to someone else, where it was first mentioned, what it stands for.", scope});
        }), FALSE, FALSE, 0);
        add(line);
    }
    if (!feature.error().empty()) add(Widgets::label(feature.error()));

    if (!feature.isWorking()) {
        add(Widgets::separator());
        const auto& passages = feature.passages();
        std::string caption = passages.empty()
            ? "Not met yet in what has been read."
            : "Where it has appeared so far" + std::string(link.jump ? " — tap to go there." : ".");
        add(Widgets::markup(Widgets::small(Widgets::escape(caption))));
        bool severalBooks = feature.scope().corpus && feature.scope().corpus->severalBooks();
        for (const auto& hit : passages) {
            add(HitView::create(hit, severalBooks, link.jump));
            add(Widgets::separator());
        }
    }
    gtk_widget_show_all(box);
    return box;
}

}  // namespace

void open(Env& env, Navigator& navigator, const std::string& term, const ReaderLink& link) {
    auto* feature = new XRayFeature(env, term, link.scope);
    GtkWidget* body = Widgets::scrolled(rendered(env, navigator, *feature, link));
    GtkWidget* screen = Widgets::screen("X-ray: " + term, body, [&navigator] { navigator.pop(); });
    Widgets::own(screen, feature);

    feature->onChange = [&env, &navigator, feature, body, link] {
        GtkWidget* viewport = gtk_bin_get_child(GTK_BIN(body));
        gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(viewport)));
        gtk_container_add(GTK_CONTAINER(viewport), rendered(env, navigator, *feature, link));
    };

    navigator.push(screen);
    feature->start();
}

void ask(Env& env, Navigator& navigator, const ReaderLink& link) {
    GtkWidget* entry = Widgets::entry("");
    auto submit = [&env, &navigator, entry, link] {
        std::string term = Text::trim(Widgets::entryText(entry));
        if (!term.empty()) open(env, navigator, term, link);
    };
    Widgets::connect(entry, "activate", submit);
    GtkWidget* composer = gtk_hbox_new(FALSE, Widgets::px(8));
    gtk_container_set_border_width(GTK_CONTAINER(composer), Widgets::px(8));
    gtk_box_pack_start(GTK_BOX(composer), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(composer), Widgets::button("X-ray", submit), FALSE, FALSE, 0);

    GtkWidget* hint = Widgets::markup("<i>" + Widgets::escape(
        "A name, a place, a word the book uses its own way: what the book has said about it so far.") + "</i>");
    gtk_misc_set_padding(GTK_MISC(hint), Widgets::px(12), Widgets::px(8));

    GtkWidget* body = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), composer, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), hint, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Keyboard::create(body), FALSE, FALSE, 0);

    navigator.push(Widgets::screen("X-ray", body, [&navigator] { navigator.pop(); }));
    gtk_widget_grab_focus(entry);
}

}  // namespace XRayView
