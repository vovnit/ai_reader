#include "GlossaryView.hpp"

#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"
#include "../Reader/ReaderFeature.hpp"
#include "GlossaryFeature.hpp"

namespace GlossaryView {

namespace {

GtkWidget* rendered(GlossaryFeature& feature) {
    GtkWidget* box = gtk_vbox_new(FALSE, Widgets::px(14));
    gtk_container_set_border_width(GTK_CONTAINER(box), Widgets::px(12));
    auto add = [&](GtkWidget* widget) { gtk_box_pack_start(GTK_BOX(box), widget, FALSE, FALSE, 0); };

    add(Widgets::label("The model is asked, once, what each word of this book means where it stands. "
                       "Its answers become the dictionary “" + feature.name() + "”, so lookups in this book "
                       "work without a network."));
    if (!feature.isCounted()) {
        add(Widgets::label("Counting words…"));
    } else {
        add(Widgets::label(feature.status()));
        if (!feature.error().empty()) add(Widgets::label(feature.error()));
        GtkWidget* line = gtk_hbox_new(FALSE, 0);
        // Later, as the screen is built anew and this button with it.
        GlossaryFeature* run = &feature;
        if (feature.isRunning()) {
            gtk_box_pack_start(GTK_BOX(line), Widgets::button("Stop", [run] { Widgets::later([run] { run->stop(); }); }), FALSE, FALSE, 0);
        } else if (feature.defined() < feature.total()) {
            std::string label = feature.defined() > 0 ? "Continue" : "Write glossary";
            gtk_box_pack_start(GTK_BOX(line), Widgets::button(label, [run] { Widgets::later([run] { run->start(); }); }), FALSE, FALSE, 0);
        }
        add(line);
    }
    gtk_widget_show_all(box);
    return box;
}

}  // namespace

void open(Env& env, Navigator& navigator, ReaderFeature& reader) {
    auto* feature = new GlossaryFeature(env, reader.book(), reader.chapterTexts(), reader.language());
    GtkWidget* body = Widgets::scrolled(rendered(*feature));
    GtkWidget* screen = Widgets::screen("Offline glossary", body, [&navigator] { navigator.pop(); });
    Widgets::own(screen, feature);

    feature->onChange = [feature, body] {
        GtkWidget* viewport = gtk_bin_get_child(GTK_BIN(body));
        gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(viewport)));
        gtk_container_add(GTK_CONTAINER(viewport), rendered(*feature));
    };

    navigator.push(screen);
    feature->count();
}

}  // namespace GlossaryView
