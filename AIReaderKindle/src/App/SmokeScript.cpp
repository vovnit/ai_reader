#include "SmokeScript.hpp"

#include "../Support/Files.hpp"
#include "../Support/Text.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace SmokeScript {

namespace {

struct Runner {
    GtkWidget* window;
    std::vector<std::string> lines;
    size_t next = 0;
};

/// The deepest visible widget under a point, in window coordinates.
GtkWidget* widgetAt(GtkWidget* widget, int x, int y) {
    if (!gtk_widget_get_visible(widget)) return nullptr;
    const GtkAllocation& box = widget->allocation;
    if (x < box.x || y < box.y || x >= box.x + box.width || y >= box.y + box.height) return nullptr;
    GtkWidget* found = widget;
    if (GTK_IS_NOTEBOOK(widget)) {
        int page = gtk_notebook_get_current_page(GTK_NOTEBOOK(widget));
        GtkWidget* current = gtk_notebook_get_nth_page(GTK_NOTEBOOK(widget), page);
        if (GtkWidget* deeper = current ? widgetAt(current, x, y) : nullptr) return deeper;
        return found;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child; child = child->next) {
            // Children of a scrolled viewport carry allocations relative to
            // the viewport's own window, so translate the point for them.
            int cx = x, cy = y;
            if (GTK_IS_VIEWPORT(widget)) {
                GtkAdjustment* vertical = gtk_viewport_get_vadjustment(GTK_VIEWPORT(widget));
                cx = x - box.x;
                cy = y - box.y + static_cast<int>(gtk_adjustment_get_value(vertical));
            }
            if (GtkWidget* deeper = widgetAt(GTK_WIDGET(child->data), cx, cy)) found = deeper;
        }
        g_list_free(children);
    }
    return found;
}

/// A dialog on top takes the taps meant for the window behind it.
GtkWidget* frontmost(GtkWidget* window) {
    GtkWidget* front = window;
    GList* toplevels = gtk_window_list_toplevels();
    for (GList* item = toplevels; item; item = item->next) {
        GtkWidget* candidate = GTK_WIDGET(item->data);
        if (GTK_IS_DIALOG(candidate) && gtk_widget_get_visible(candidate)) front = candidate;
    }
    g_list_free(toplevels);
    return front;
}

void tap(GtkWidget* window, int x, int y) {
    window = frontmost(window);
    GtkWidget* target = widgetAt(window, x, y);
    while (target && !GTK_IS_BUTTON(target) && !GTK_IS_DRAWING_AREA(target) && !GTK_IS_ENTRY(target)
           && !GTK_IS_TOGGLE_BUTTON(target)) {
        target = gtk_widget_get_parent(target);
    }
    if (!target) {
        std::printf("smoke: nothing at %d,%d\n", x, y);
        return;
    }
    if (GTK_IS_ENTRY(target)) {
        gtk_widget_grab_focus(target);
        return;
    }
    if (GTK_IS_BUTTON(target)) {
        gtk_button_clicked(GTK_BUTTON(target));
        return;
    }
    GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);
    event->button.window = target->window;
    g_object_ref(event->button.window);
    event->button.x = x - target->allocation.x;
    event->button.y = y - target->allocation.y;
    event->button.button = 1;
    event->button.time = GDK_CURRENT_TIME;
    gtk_widget_event(target, event);
    gdk_event_free(event);
}

/// Whether any label inside the widget reads `text`, on any of its lines.
bool reads(GtkWidget* widget, const std::string& text) {
    if (GTK_IS_LABEL(widget)) {
        for (const auto& line : Text::split(gtk_label_get_text(GTK_LABEL(widget)), '\n')) {
            if (Text::trim(line) == text) return true;
        }
        return false;
    }
    bool found = false;
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child && !found; child = child->next) found = reads(GTK_WIDGET(child->data), text);
        g_list_free(children);
    }
    return found;
}

/// The first visible button reading `label`, on the screen in front.
GtkWidget* buttonReading(GtkWidget* widget, const std::string& label) {
    if (!gtk_widget_get_visible(widget)) return nullptr;
    if (GTK_IS_BUTTON(widget) && reads(widget, label)) return widget;
    if (GTK_IS_NOTEBOOK(widget)) {
        int page = gtk_notebook_get_current_page(GTK_NOTEBOOK(widget));
        GtkWidget* current = gtk_notebook_get_nth_page(GTK_NOTEBOOK(widget), page);
        return current ? buttonReading(current, label) : nullptr;
    }
    GtkWidget* found = nullptr;
    if (GTK_IS_CONTAINER(widget)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList* child = children; child && !found; child = child->next) found = buttonReading(GTK_WIDGET(child->data), label);
        g_list_free(children);
    }
    return found;
}

void press(GtkWidget* window, const std::string& label) {
    GtkWidget* button = buttonReading(frontmost(window), label);
    if (button) gtk_button_clicked(GTK_BUTTON(button));
    else std::printf("smoke: no button reads \"%s\"\n", label.c_str());
}

void snap(GtkWidget* window, const std::string& path) {
    // Let pending draws land first.
    while (gtk_events_pending()) gtk_main_iteration();
    int width = window->allocation.width;
    int height = window->allocation.height;
    GdkPixbuf* pixbuf = gdk_pixbuf_get_from_drawable(nullptr, window->window, nullptr, 0, 0, 0, 0, width, height);
    if (!pixbuf) {
        std::printf("smoke: could not capture the window\n");
        return;
    }
    GError* error = nullptr;
    gdk_pixbuf_save(pixbuf, path.c_str(), "png", &error, nullptr);
    if (error) {
        std::printf("smoke: %s\n", error->message);
        g_error_free(error);
    }
    g_object_unref(pixbuf);
}

/// Prints the visible widget tree with window coordinates and text.
void dump(GtkWidget* widget, int depth, int dx, int dy) {
    if (!gtk_widget_get_visible(widget)) return;
    const GtkAllocation& box = widget->allocation;
    std::string text;
    if (GTK_IS_LABEL(widget)) text = gtk_label_get_text(GTK_LABEL(widget));
    else if (GTK_IS_ENTRY(widget)) text = gtk_entry_get_text(GTK_ENTRY(widget));
    if (GTK_IS_IMAGE(widget)) text = gtk_image_get_pixbuf(GTK_IMAGE(widget)) ? "(image)" : "(empty)";
    if (GTK_IS_TOGGLE_BUTTON(widget)) text = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ? "[x]" : "[ ]";
    std::string name = G_OBJECT_TYPE_NAME(widget);
    if (!(name == "GtkVBox" || name == "GtkHBox" || name == "GtkAlignment" || name == "GtkViewport"
          || name == "GtkScrolledWindow" || name == "GtkHSeparator" || name == "GtkNotebook")) {
        std::printf("%*s%s %d,%d %dx%d %s\n", depth * 2, "", name.c_str(), box.x + dx, box.y + dy,
                    box.width, box.height, Text::replaceAll(text, "\n", " | ").c_str());
    }
    if (GTK_IS_NOTEBOOK(widget)) {
        int page = gtk_notebook_get_current_page(GTK_NOTEBOOK(widget));
        if (GtkWidget* current = gtk_notebook_get_nth_page(GTK_NOTEBOOK(widget), page)) dump(current, depth + 1, dx, dy);
        return;
    }
    if (!GTK_IS_CONTAINER(widget)) return;
    int cx = dx, cy = dy;
    if (GTK_IS_VIEWPORT(widget)) {
        cx += box.x;
        cy += box.y - static_cast<int>(gtk_adjustment_get_value(gtk_viewport_get_vadjustment(GTK_VIEWPORT(widget))));
    }
    GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
    for (GList* child = children; child; child = child->next) dump(GTK_WIDGET(child->data), depth + 1, cx, cy);
    g_list_free(children);
}

gboolean step(gpointer data) {
    auto* runner = static_cast<Runner*>(data);
    while (runner->next < runner->lines.size()) {
        std::string line = Text::trim(runner->lines[runner->next++]);
        if (line.empty() || line[0] == '#') continue;
        auto space = line.find(' ');
        std::string command = line.substr(0, space);
        std::string argument = space == std::string::npos ? "" : Text::trim(line.substr(space + 1));
        std::printf("smoke: %s\n", line.c_str());

        if (command == "wait") {
            g_timeout_add(static_cast<guint>(std::atoi(argument.c_str())), step, runner);
            return FALSE;  // one-shot
        } else if (command == "tap") {
            int x = 0, y = 0;
            std::sscanf(argument.c_str(), "%d %d", &x, &y);
            tap(runner->window, x, y);
        } else if (command == "press") {
            press(runner->window, argument);
        } else if (command == "type") {
            GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(runner->window));
            if (GTK_IS_ENTRY(focus)) gtk_entry_set_text(GTK_ENTRY(focus), argument.c_str());
            else std::printf("smoke: no entry has focus\n");
        } else if (command == "dump") {
            while (gtk_events_pending()) gtk_main_iteration();
            dump(frontmost(runner->window), 0, 0, 0);
        } else if (command == "snap") {
            snap(runner->window, argument);
        } else if (command == "quit") {
            gtk_main_quit();
            break;
        }
    }
    delete runner;
    return FALSE;  // one-shot
}

}  // namespace

void start(GtkWidget* window) {
    const char* path = g_getenv("AIREADER_SCRIPT");
    if (!path) return;
    auto script = Files::read(path);
    if (!script) return;
    auto* runner = new Runner{window, Text::split(*script, '\n'), 0};
    g_timeout_add(500, step, runner);
}

}  // namespace SmokeScript
