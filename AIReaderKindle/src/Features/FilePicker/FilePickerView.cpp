#include "FilePickerView.hpp"

#include "../../Services/Paths.hpp"
#include "../Common/Navigator.hpp"
#include "../Common/Widgets.hpp"
#include "FilePickerFeature.hpp"

#include <memory>

namespace FilePickerView {

namespace {

struct Screen {
    FilePickerFeature feature;
    Navigator& navigator;
    std::shared_ptr<std::function<void(const std::string&)>> onPick;
    GtkWidget* location = nullptr;
    GtkWidget* list = nullptr;
    GtkWidget* up = nullptr;

    Screen(Navigator& navigator, std::function<bool(const std::string&)> accepts,
           std::function<void(const std::string&)> onPick)
        : feature(Paths::browseRoot(), std::move(accepts)), navigator(navigator),
          onPick(std::make_shared<std::function<void(const std::string&)>>(std::move(onPick))) {}

    GtkWidget* row(const FilePickerFeature::Entry& entry) {
        std::string text = Widgets::escape(entry.name);
        if (entry.isFolder) text = "<b>" + text + "</b>";
        GtkWidget* line = gtk_hbox_new(FALSE, Widgets::px(8));
        gtk_box_pack_start(GTK_BOX(line), Widgets::markup(text), TRUE, TRUE, 0);
        if (entry.isFolder) gtk_box_pack_end(GTK_BOX(line), Widgets::label("›", 1, false), FALSE, FALSE, 0);
        GtkWidget* button = Widgets::flatButton(line, [this, entry] {
            if (entry.isFolder) {
                feature.enter(entry);
            } else {
                // The picker is gone by the time the caller runs.
                auto pick = onPick;
                std::string path = entry.path;
                navigator.pop();
                Widgets::later([pick, path] { (*pick)(path); });
            }
        });
        gtk_widget_set_size_request(button, -1, Widgets::px(48));
        return button;
    }

    void render() {
        gtk_label_set_text(GTK_LABEL(location), feature.directory().c_str());
        gtk_widget_set_sensitive(up, feature.canGoUp());
        GList* children = gtk_container_get_children(GTK_CONTAINER(list));
        for (GList* child = children; child; child = child->next) gtk_widget_destroy(GTK_WIDGET(child->data));
        g_list_free(children);
        if (feature.entries().empty()) {
            gtk_box_pack_start(GTK_BOX(list), Widgets::label("Nothing here to add."), FALSE, FALSE, Widgets::px(12));
        }
        for (const auto& entry : feature.entries()) {
            gtk_box_pack_start(GTK_BOX(list), row(entry), FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(list), Widgets::separator(), FALSE, FALSE, 0);
        }
        gtk_widget_show_all(list);
    }
};

}  // namespace

void open(
    Navigator& navigator,
    const std::string& title,
    std::function<bool(const std::string& path)> accepts,
    std::function<void(const std::string& path)> onPick)
{
    auto* screen = new Screen(navigator, std::move(accepts), std::move(onPick));

    // The folder's path above the list, shortened from the front so the
    // folder's own name is always visible.
    screen->location = Widgets::label("", 0, false);
    gtk_label_set_ellipsize(GTK_LABEL(screen->location), PANGO_ELLIPSIZE_START);
    gtk_misc_set_padding(GTK_MISC(screen->location), Widgets::px(12), Widgets::px(8));
    screen->list = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(screen->list), Widgets::px(8));

    GtkWidget* body = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), screen->location, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(body), Widgets::scrolled(screen->list), TRUE, TRUE, 0);

    screen->up = Widgets::button("Up", [screen] { screen->feature.up(); });
    GtkWidget* widget = Widgets::screen(title, body, [&navigator] { navigator.pop(); }, "Cancel", {screen->up});
    Widgets::own(widget, screen);
    screen->feature.onChange = [screen] { screen->render(); };
    screen->render();
    navigator.push(widget);
}

}  // namespace FilePickerView
