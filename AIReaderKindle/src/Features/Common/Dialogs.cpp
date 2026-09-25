#include "Widgets.hpp"

#include "../../Services/Paths.hpp"
#include "Navigator.hpp"
#include "Theme.hpp"

#include <memory>

/// The screens and windows that stand apart from the screen behind them:
/// the full-screen picker, message dialogs, and the window title the
/// Kindle's window manager reads.
namespace Widgets {

void picker(
    Navigator& navigator,
    const std::string& title,
    const std::vector<std::string>& options,
    const std::string& selected,
    std::function<void(const std::string&)> onPick)
{
    GtkWidget* list = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(list), px(8));
    auto pick = std::make_shared<std::function<void(const std::string&)>>(std::move(onPick));
    for (const auto& option : options) {
        GtkWidget* line = gtk_hbox_new(FALSE, px(8));
        gtk_box_pack_start(GTK_BOX(line), label(option, 0, false), TRUE, TRUE, 0);
        if (option == selected) gtk_box_pack_end(GTK_BOX(line), label("✓", 1, false), FALSE, FALSE, 0);
        // The pick lands after the pop, so what it opens goes on top of
        // the screen the picker came from.
        GtkWidget* row = flatButton(line, [&navigator, pick, option] {
            navigator.pop();
            later([pick, option] { (*pick)(option); });
        });
        gtk_widget_set_size_request(row, -1, px(48));
        gtk_box_pack_start(GTK_BOX(list), row, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(list), separator(), FALSE, FALSE, 0);
    }
    navigator.push(screen(title, scrolled(list), [&navigator] { navigator.pop(); }));
}

namespace {

void freeAction(gpointer data, GClosure*) {
    delete static_cast<Action*>(data);
}

GtkWidget* dialog(GtkWindow* parent, const std::string& title, const std::string& message) {
    GtkWidget* widget = gtk_message_dialog_new(
        parent, GTK_DIALOG_MODAL, GTK_MESSAGE_OTHER, GTK_BUTTONS_NONE, "%s", title.c_str());
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(widget), "%s", message.c_str());
    // GTK+ 2.22 dropped the line above the buttons; the Kindle's 2.20 draws
    // it, etched, unless told not to.
    if (gtk_check_version(2, 22, 0) != nullptr) gtk_dialog_set_has_separator(GTK_DIALOG(widget), FALSE);
    kindleTitle(GTK_WINDOW(widget), "D", "dialog", "");
    return widget;
}

/// Adds a button in the theme's shape. The dialog's own would be bevelled.
void addButton(GtkWidget* dialog, const std::string& label, int response) {
    GtkWidget* widget = gtk_dialog_add_button(GTK_DIALOG(dialog), label.c_str(), response);
    gtk_widget_set_size_request(widget, px(96), px(40));
    Theme::styleButton(widget);
}

void responded(GtkDialog* widget, gint response, gpointer data) {
    auto* onYes = static_cast<Action*>(data);
    if (response == GTK_RESPONSE_ACCEPT && *onYes) (*onYes)();
    gtk_widget_destroy(GTK_WIDGET(widget));
}

}  // namespace

void alert(GtkWindow* parent, const std::string& title, const std::string& message) {
    GtkWidget* widget = dialog(parent, title, message);
    addButton(widget, "OK", GTK_RESPONSE_ACCEPT);
    g_signal_connect(widget, "response", G_CALLBACK(gtk_widget_destroy), nullptr);
    gtk_widget_show(widget);
}

void confirm(GtkWindow* parent, const std::string& title, const std::string& message, const std::string& yes, Action onYes) {
    GtkWidget* widget = dialog(parent, title, message);
    addButton(widget, "Cancel", GTK_RESPONSE_CANCEL);
    addButton(widget, yes, GTK_RESPONSE_ACCEPT);
    g_signal_connect_data(widget, "response", G_CALLBACK(responded), new Action(std::move(onYes)), freeAction, GConnectFlags(0));
    gtk_widget_show(widget);
}

void kindleTitle(GtkWindow* window, const char* layer, const char* role, const char* extra) {
    if (!Paths::onKindle()) {
        gtk_window_set_title(window, "AIReader");
        return;
    }
    std::string title = std::string("L:") + layer + "_N:" + role + "_ID:" + AIREADER_APP_ID + extra;
    gtk_window_set_title(window, title.c_str());
}

}  // namespace Widgets
