#include "DictionariesView.hpp"

#include "../Common/Navigator.hpp"
#include "../../Services/Paths.hpp"
#include "../../Domain/Formats/DictionaryFormat.hpp"
#include "../Common/Widgets.hpp"
#include "../FilePicker/FilePickerView.hpp"
#include "DictionariesFeature.hpp"

namespace DictionariesView {

namespace {

GtkWidget* row(DictionariesFeature& feature, const DictionaryPack& pack) {
    GtkWidget* line = gtk_hbox_new(FALSE, Widgets::px(8));
    // Deferred: the row, check included, is rebuilt when the list changes.
    GtkWidget* check = Widgets::check(pack.isEnabled, [&feature, pack](bool) {
        Widgets::later([&feature, pack] { feature.toggle(pack); });
    });
    gtk_box_pack_start(GTK_BOX(line), check, FALSE, FALSE, 0);

    std::string text = "<b>" + Widgets::escape(pack.name) + "</b>";
    std::string detail = pack.languages();
    if (pack.isBundled()) detail += detail.empty() ? "Ships with the app" : " · ships with the app";
    if (!detail.empty()) text += "\n" + Widgets::small(Widgets::escape(detail));
    gtk_box_pack_start(GTK_BOX(line), Widgets::markup(text), TRUE, TRUE, 0);

    if (!pack.isBundled()) {
        GtkWidget* remove = Widgets::glyphButton("✕", [&feature, pack] {
            Widgets::later([&feature, pack] { feature.remove(pack); });
        }, true);
        gtk_box_pack_end(GTK_BOX(line), remove, FALSE, FALSE, 0);
    }
    return line;
}

/// Converting a big dictionary takes a while and blocks the screen; the
/// button says so before the work starts.
void busy(GtkWidget* button, const std::string& label, const std::function<std::string()>& work, Navigator& navigator) {
    std::string was = gtk_button_get_label(GTK_BUTTON(button));
    gtk_button_set_label(GTK_BUTTON(button), label.c_str());
    gtk_widget_set_sensitive(button, FALSE);
    while (gtk_events_pending()) gtk_main_iteration();
    std::string message = work();
    gtk_button_set_label(GTK_BUTTON(button), was.c_str());
    gtk_widget_set_sensitive(button, TRUE);
    Widgets::alert(navigator.window(), "Dictionaries", message);
}

}  // namespace

void open(Env& env, Navigator& navigator) {
    auto* feature = new DictionariesFeature(env);
    GtkWidget* list = gtk_vbox_new(FALSE, Widgets::px(12));
    gtk_container_set_border_width(GTK_CONTAINER(list), Widgets::px(12));

    auto render = [feature, list] {
        GList* children = gtk_container_get_children(GTK_CONTAINER(list));
        for (GList* child = children; child; child = child->next) gtk_widget_destroy(GTK_WIDGET(child->data));
        g_list_free(children);
        for (const auto& pack : feature->packs()) {
            gtk_box_pack_start(GTK_BOX(list), row(*feature, pack), FALSE, FALSE, 0);
        }
        gtk_box_pack_start(GTK_BOX(list), Widgets::separator(), FALSE, FALSE, Widgets::px(6));
        gtk_box_pack_start(GTK_BOX(list), Widgets::markup(Widgets::small(
            "Tap “Add” to pick a dictionary on this device: a pack (<tt>.sqlite3</tt>), a word list "
            "(<tt>.tsv</tt>, <tt>.csv</tt>: one headword and its meaning per line), Lingvo DSL, StarDict or XDXF. "
            "Or copy files into\n<tt>" + Widgets::escape(Paths::dictionaries()) + "</tt>\n"
            "and tap “Scan folder”.")), FALSE, FALSE, 0);
        gtk_widget_show_all(list);
    };
    feature->onChange = render;
    render();

    GtkWidget* scan = Widgets::button("Scan folder", nullptr);
    Widgets::connect(scan, "clicked", [feature, &navigator, scan] {
        busy(scan, "Adding…", [feature] { return feature->addFromFolder(); }, navigator);
    });
    GtkWidget* add = Widgets::button("Add", nullptr);
    Widgets::connect(add, "clicked", [feature, &navigator, add] {
        FilePickerView::open(navigator, "Add a dictionary",
            [](const std::string& path) { return DictionaryFormats::of(path).has_value(); },
            [feature, &navigator, add](const std::string& path) {
                busy(add, "Adding…", [feature, path] { return feature->addFile(path); }, navigator);
            });
    });
    GtkWidget* screen = Widgets::screen("Dictionaries", Widgets::scrolled(list), [&navigator] { navigator.pop(); }, "Back", {scan, add});
    Widgets::own(screen, feature);
    navigator.push(screen);
}

}  // namespace DictionariesView
