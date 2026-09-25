#include "Widgets.hpp"

#include "Theme.hpp"

#include <algorithm>
#include <cmath>

namespace Widgets {

namespace {

int wrapWidth_ = 560;
double scale_ = 1.0;
int fontPixels_ = 0;

std::string sized(double pixels, const std::string& markup) {
    return "<span font_desc=\"" + std::to_string(std::lround(pixels)) + "px\">" + markup + "</span>";
}

void trampoline(GtkWidget*, gpointer data) {
    (*static_cast<Action*>(data))();
}

void freeAction(gpointer data, GClosure*) {
    delete static_cast<Action*>(data);
}

}  // namespace

void connect(GtkWidget* widget, const char* signal, Action action) {
    g_signal_connect_data(widget, signal, G_CALLBACK(trampoline), new Action(std::move(action)), freeAction, GConnectFlags(0));
}

void later(Action action) {
    g_idle_add([](gpointer data) -> gboolean {
        auto* stored = static_cast<Action*>(data);
        (*stored)();
        delete stored;
        return FALSE;  // one-shot
    }, new Action(std::move(action)));
}

double scale() { return scale_; }
void setScale(double scale) { scale_ = scale; }
int px(int desktopPixels) { return static_cast<int>(std::lround(desktopPixels * scale_)); }

int wrapWidth() { return wrapWidth_; }
void setWrapWidth(int width) { wrapWidth_ = width; }

int fontPixels() { return fontPixels_ > 0 ? fontPixels_ : px(15); }
void setFontPixels(int pixels) { fontPixels_ = pixels; }
// The steps Pango's own <small> and <big> take.
std::string small(const std::string& markup) { return sized(fontPixels() / 1.2, markup); }
std::string big(const std::string& markup) { return sized(fontPixels() * 1.2, markup); }

GtkWidget* button(const std::string& text, Action action) {
    GtkWidget* widget = gtk_button_new_with_label(text.c_str());
    gtk_widget_set_size_request(widget, -1, px(40));
    gtk_button_set_focus_on_click(GTK_BUTTON(widget), FALSE);
    Theme::styleButton(widget);
    if (action) connect(widget, "clicked", std::move(action));
    return widget;
}

GtkWidget* glyphButton(const std::string& glyph, Action action, bool flat) {
    GtkWidget* widget = button("", std::move(action));
    gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(widget))), glyph.c_str());
    gtk_widget_set_size_request(widget, px(44), px(40));
    if (flat) gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NONE);
    return widget;
}

GtkWidget* contentButton(GtkWidget* content, Action action) {
    GtkWidget* widget = gtk_button_new();
    gtk_button_set_focus_on_click(GTK_BUTTON(widget), FALSE);
    gtk_container_add(GTK_CONTAINER(widget), content);
    Theme::styleButton(widget);
    connect(widget, "clicked", std::move(action));
    return widget;
}

GtkWidget* flatButton(GtkWidget* content, Action action) {
    GtkWidget* widget = contentButton(content, std::move(action));
    gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NONE);
    return widget;
}

GtkWidget* check(bool active, std::function<void(bool)> onToggle) {
    GtkWidget* widget = gtk_toggle_button_new();
    gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click(GTK_BUTTON(widget), FALSE);
    gtk_widget_set_size_request(widget, px(44), px(44));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), active);
    Theme::styleCheck(widget);
    connect(widget, "toggled", [widget, onToggle = std::move(onToggle)] {
        onToggle(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
    });
    return widget;
}

namespace {

gboolean applyWrapWidth(gpointer data) {
    GtkWidget* label = GTK_WIDGET(data);
    int width = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(label), "aireader-wrap-width"));
    gtk_widget_set_size_request(label, width, -1);
    g_object_unref(label);
    return FALSE;  // one-shot
}

/// GTK+ 2 wraps a label at the width it asked for, not the width it was
/// given, so a label asks for a little and then, once allocated, asks for
/// exactly what it got. That fills the row without ever growing the window.
void fitWrap(GtkWidget* label, GtkAllocation* allocation, gpointer) {
    int requested = -1;
    gtk_widget_get_size_request(label, &requested, nullptr);
    if (allocation->width <= 1 || allocation->width == requested) return;
    g_object_set_data(G_OBJECT(label), "aireader-wrap-width", GINT_TO_POINTER(allocation->width));
    g_object_ref(label);
    g_idle_add(applyWrapWidth, label);
}

}  // namespace

static GtkWidget* configureLabel(GtkWidget* widget, double xalign, bool wrap) {
    gtk_misc_set_alignment(GTK_MISC(widget), static_cast<gfloat>(xalign), 0);
    if (wrap) {
        gtk_label_set_line_wrap(GTK_LABEL(widget), TRUE);
        gtk_label_set_line_wrap_mode(GTK_LABEL(widget), PANGO_WRAP_WORD_CHAR);
        gtk_widget_set_size_request(widget, std::min(wrapWidth_, px(120)), -1);
        g_signal_connect(widget, "size-allocate", G_CALLBACK(fitWrap), nullptr);
    }
    return widget;
}

GtkWidget* label(const std::string& text, double xalign, bool wrap) {
    return configureLabel(gtk_label_new(text.c_str()), xalign, wrap);
}

GtkWidget* markup(const std::string& text, double xalign, bool wrap) {
    GtkWidget* widget = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(widget), text.c_str());
    return configureLabel(widget, xalign, wrap);
}

std::string escape(const std::string& text) {
    gchar* escaped = g_markup_escape_text(text.c_str(), static_cast<gssize>(text.size()));
    std::string result = escaped;
    g_free(escaped);
    return result;
}

namespace {

/// A finger dragging on the list scrolls it, the way a touch screen expects.
/// Buttons take their own presses, so drags start on text or empty space.
struct Drag {
    bool active = false;
    double startY = 0;
    double startValue = 0;
};

gboolean dragPress(GtkWidget* viewport, GdkEventButton* event, gpointer data) {
    auto* drag = static_cast<Drag*>(data);
    drag->active = true;
    drag->startY = event->y_root;
    drag->startValue = gtk_adjustment_get_value(gtk_viewport_get_vadjustment(GTK_VIEWPORT(viewport)));
    return FALSE;
}

gboolean dragMove(GtkWidget* viewport, GdkEventMotion* event, gpointer data) {
    auto* drag = static_cast<Drag*>(data);
    if (!drag->active) return FALSE;
    GtkAdjustment* adjustment = gtk_viewport_get_vadjustment(GTK_VIEWPORT(viewport));
    double value = drag->startValue - (event->y_root - drag->startY);
    double top = gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment);
    gtk_adjustment_set_value(adjustment, std::max(0.0, std::min(value, top)));
    return TRUE;
}

gboolean dragRelease(GtkWidget*, GdkEventButton*, gpointer data) {
    static_cast<Drag*>(data)->active = false;
    return FALSE;
}

}  // namespace

GtkWidget* scrolled(GtkWidget* child) {
    GtkWidget* window = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(window), child);
    GtkWidget* viewport = gtk_bin_get_child(GTK_BIN(window));
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(window), GTK_SHADOW_NONE);
    Theme::styleScrollbar(gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(window)));

    auto* drag = new Drag;
    own(viewport, drag);
    gtk_widget_add_events(viewport, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    g_signal_connect(viewport, "button-press-event", G_CALLBACK(dragPress), drag);
    g_signal_connect(viewport, "motion-notify-event", G_CALLBACK(dragMove), drag);
    g_signal_connect(viewport, "button-release-event", G_CALLBACK(dragRelease), drag);
    return window;
}

GtkWidget* separator() {
    GtkWidget* widget = gtk_hseparator_new();
    Theme::styleSeparator(widget);
    return widget;
}

GtkWidget* screen(
    const std::string& title,
    GtkWidget* body,
    Action onBack,
    const std::string& backLabel,
    const std::vector<GtkWidget*>& actions)
{
    GtkWidget* header = gtk_hbox_new(FALSE, px(8));
    gtk_container_set_border_width(GTK_CONTAINER(header), px(8));
    if (onBack) gtk_box_pack_start(GTK_BOX(header), button(backLabel, std::move(onBack)), FALSE, FALSE, 0);
    GtkWidget* heading = markup("<b>" + escape(title) + "</b>", 0, false);
    gtk_label_set_ellipsize(GTK_LABEL(heading), PANGO_ELLIPSIZE_END);
    gtk_misc_set_alignment(GTK_MISC(heading), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(header), heading, TRUE, TRUE, px(4));
    for (GtkWidget* action : actions) gtk_box_pack_end(GTK_BOX(header), action, FALSE, FALSE, 0);

    GtkWidget* box = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), separator(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), body, TRUE, TRUE, 0);
    gtk_widget_show_all(box);
    return box;
}

GtkWidget* entry(const std::string& text, bool secret) {
    GtkWidget* widget = gtk_entry_new();
    gtk_widget_set_size_request(widget, -1, px(40));
    Theme::styleEntry(widget);
    gtk_entry_set_text(GTK_ENTRY(widget), text.c_str());
    if (secret) gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
    return widget;
}

std::string entryText(GtkWidget* widget) {
    return gtk_entry_get_text(GTK_ENTRY(widget));
}

}  // namespace Widgets
