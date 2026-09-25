#include "Theme.hpp"

#include "Widgets.hpp"

#include <algorithm>

namespace Theme {

namespace {

const double ink = 0.0;
const double paper = 1.0;
const double rule = 0.72;   // separators, scrollbars
const double muted = 0.56;  // outlines of what cannot be tapped

std::string n(int desktopPixels) { return std::to_string(Widgets::px(desktopPixels)); }

/// Where the widget sits on the window it draws on: widgets without a
/// window of their own draw at their allocation on the parent's.
GdkRectangle bounds(GtkWidget* widget) {
    GdkRectangle box = {0, 0, widget->allocation.width, widget->allocation.height};
    if (!gtk_widget_get_has_window(widget)) {
        box.x = widget->allocation.x;
        box.y = widget->allocation.y;
    }
    return box;
}

cairo_t* begin(GdkEventExpose* event) {
    cairo_t* cr = gdk_cairo_create(event->window);
    gdk_cairo_region(cr, event->region);
    cairo_clip(cr);
    return cr;
}

void roundedRectangle(cairo_t* cr, double x, double y, double width, double height, double radius) {
    radius = std::min(radius, std::min(width, height) / 2);
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -G_PI / 2, 0);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0, G_PI / 2);
    cairo_arc(cr, x + radius, y + height - radius, radius, G_PI / 2, G_PI);
    cairo_arc(cr, x + radius, y + radius, radius, G_PI, 3 * G_PI / 2);
    cairo_close_path(cr);
}

/// An outline inset by half its width, so it is not clipped.
void outline(cairo_t* cr, const GdkRectangle& box, double line, double radius) {
    roundedRectangle(cr, box.x + line / 2, box.y + line / 2, box.width - line, box.height - line, radius);
}

const char* const selectedKey = "aireader-selected";

bool isSelected(GtkWidget* button) {
    return g_object_get_data(G_OBJECT(button), selectedKey) != nullptr;
}

/// Labels draw in their own colour, so those on a filled button are turned
/// white by hand; `nullptr` gives them back to the style.
void tintLabels(GtkWidget* widget, const GdkColor* colour) {
    if (GTK_IS_LABEL(widget)) {
        for (GtkStateType state : {GTK_STATE_NORMAL, GTK_STATE_PRELIGHT, GTK_STATE_ACTIVE}) {
            gtk_widget_modify_fg(widget, state, colour);
        }
        return;
    }
    if (!GTK_IS_CONTAINER(widget)) return;
    GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
    for (GList* child = children; child; child = child->next) tintLabels(GTK_WIDGET(child->data), colour);
    g_list_free(children);
}

gboolean buttonExpose(GtkWidget* widget, GdkEventExpose* event, gpointer) {
    bool pressed = gtk_widget_get_state(widget) == GTK_STATE_ACTIVE || isSelected(widget);
    bool outlined = gtk_button_get_relief(GTK_BUTTON(widget)) != GTK_RELIEF_NONE;
    if (pressed || outlined) {
        cairo_t* cr = begin(event);
        double line = Widgets::px(2);
        outline(cr, bounds(widget), line, Widgets::px(6));
        if (pressed) {
            cairo_set_source_rgb(cr, ink, ink, ink);
            cairo_fill(cr);
        } else {
            double tone = gtk_widget_is_sensitive(widget) ? ink : muted;
            cairo_set_source_rgb(cr, paper, paper, paper);
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, tone, tone, tone);
            cairo_set_line_width(cr, line);
            cairo_stroke(cr);
        }
        cairo_destroy(cr);
    }
    if (GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget))) {
        gtk_container_propagate_expose(GTK_CONTAINER(widget), child, event);
    }
    return TRUE;
}

/// An entry exposes twice, once for its frame and once for its text; only
/// the frame is taken over.
gboolean entryExpose(GtkWidget* widget, GdkEventExpose* event, gpointer) {
    if (event->window != widget->window) return FALSE;
    cairo_t* cr = begin(event);
    double line = Widgets::px(2);
    double tone = gtk_widget_has_focus(widget) ? ink : muted;
    outline(cr, bounds(widget), line, Widgets::px(6));
    cairo_set_source_rgb(cr, paper, paper, paper);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, tone, tone, tone);
    cairo_set_line_width(cr, line);
    cairo_stroke(cr);
    cairo_destroy(cr);
    return TRUE;
}

gboolean checkExpose(GtkWidget* widget, GdkEventExpose* event, gpointer) {
    bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    GdkRectangle box = bounds(widget);
    int size = Widgets::px(26);
    GdkRectangle square = {box.x + (box.width - size) / 2, box.y + (box.height - size) / 2, size, size};
    cairo_t* cr = begin(event);
    double line = Widgets::px(2);
    outline(cr, square, line, Widgets::px(5));
    if (on) {
        cairo_set_source_rgb(cr, ink, ink, ink);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, paper, paper, paper);
        cairo_set_line_width(cr, Widgets::px(3));
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_move_to(cr, square.x + size * 0.25, square.y + size * 0.52);
        cairo_line_to(cr, square.x + size * 0.43, square.y + size * 0.71);
        cairo_line_to(cr, square.x + size * 0.76, square.y + size * 0.31);
        cairo_stroke(cr);
    } else {
        cairo_set_source_rgb(cr, paper, paper, paper);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, ink, ink, ink);
        cairo_set_line_width(cr, line);
        cairo_stroke(cr);
    }
    cairo_destroy(cr);
    return TRUE;
}

gboolean scrollbarExpose(GtkWidget* widget, GdkEventExpose* event, gpointer) {
    GtkAdjustment* adjustment = gtk_range_get_adjustment(GTK_RANGE(widget));
    double lower = gtk_adjustment_get_lower(adjustment);
    double span = gtk_adjustment_get_upper(adjustment) - lower;
    double page = gtk_adjustment_get_page_size(adjustment);
    if (span <= page || page <= 0) return TRUE;
    GdkRectangle box = bounds(widget);
    int length = std::max(Widgets::px(40), static_cast<int>(box.height * page / span));
    double travel = (gtk_adjustment_get_value(adjustment) - lower) / (span - page);
    int top = box.y + static_cast<int>((box.height - length) * std::max(0.0, std::min(1.0, travel)));
    int thickness = Widgets::px(4);
    cairo_t* cr = begin(event);
    roundedRectangle(cr, box.x + (box.width - thickness) / 2.0, top, thickness, length, thickness / 2.0);
    cairo_set_source_rgb(cr, rule, rule, rule);
    cairo_fill(cr);
    cairo_destroy(cr);
    return TRUE;
}

gboolean separatorExpose(GtkWidget* widget, GdkEventExpose* event, gpointer) {
    GdkRectangle box = bounds(widget);
    int thickness = std::max(1, Widgets::px(1));
    cairo_t* cr = begin(event);
    cairo_rectangle(cr, box.x, box.y + (box.height - thickness) / 2, box.width, thickness);
    cairo_set_source_rgb(cr, rule, rule, rule);
    cairo_fill(cr);
    cairo_destroy(cr);
    return TRUE;
}

}  // namespace

void apply(const std::string& font) {
    PangoFontDescription* description = pango_font_description_from_string(font.c_str());
    if (pango_font_description_get_size_is_absolute(description)) {
        Widgets::setFontPixels(pango_font_description_get_size(description) / PANGO_SCALE);
    }
    pango_font_description_free(description);

    std::string rc =
        "style \"aireader\" {\n"
        "  font_name = \"" + font + "\"\n"
        "  fg[NORMAL] = \"#000000\" fg[PRELIGHT] = \"#000000\" fg[ACTIVE] = \"#000000\"\n"
        "  fg[SELECTED] = \"#ffffff\" fg[INSENSITIVE] = \"#8f8f8f\"\n"
        "  bg[NORMAL] = \"#ffffff\" bg[PRELIGHT] = \"#ffffff\" bg[ACTIVE] = \"#ffffff\"\n"
        "  bg[SELECTED] = \"#000000\" bg[INSENSITIVE] = \"#ffffff\"\n"
        "  base[NORMAL] = \"#ffffff\" base[SELECTED] = \"#000000\" base[ACTIVE] = \"#000000\"\n"
        "  text[NORMAL] = \"#000000\" text[SELECTED] = \"#ffffff\" text[ACTIVE] = \"#ffffff\"\n"
        "  xthickness = " + n(2) + "\n"
        "  ythickness = " + n(2) + "\n"
        "  GtkWidget::focus-line-width = 0\n"
        "  GtkWidget::interior-focus = 1\n"
        "  GtkEntry::inner-border = { " + n(10) + ", " + n(10) + ", " + n(4) + ", " + n(4) + " }\n"
        "  GtkScrollbar::has-backward-stepper = 0\n"
        "  GtkScrollbar::has-forward-stepper = 0\n"
        "  GtkScrollbar::slider-width = " + n(8) + "\n"
        "  GtkRange::trough-border = 0\n"
        "  GtkScrolledWindow::scrollbar-spacing = 0\n"
        "  GtkDialog::content-area-border = " + n(16) + "\n"
        "  GtkDialog::button-spacing = " + n(8) + "\n"
        "  GtkDialog::action-area-border = " + n(12) + "\n"
        "}\n"
        // Room around a button's label, and white text on the black fill of
        // a pressed one. The rule reaches the label inside the button too.
        "style \"aireader-button\" = \"aireader\" {\n"
        "  fg[ACTIVE] = \"#ffffff\"\n"
        "  xthickness = " + n(8) + "\n"
        "  ythickness = " + n(4) + "\n"
        "  GtkButton::inner-border = { " + n(4) + ", " + n(4) + ", " + n(2) + ", " + n(2) + " }\n"
        "  GtkButton::default-border = { 0, 0, 0, 0 }\n"
        "  GtkButton::focus-line-width = 0\n"
        "  GtkButton::child-displacement-x = 0\n"
        "  GtkButton::child-displacement-y = 0\n"
        "}\n"
        "style \"aireader-key\" = \"aireader-button\" {\n"
        "  xthickness = " + n(2) + "\n"
        "  GtkButton::inner-border = { 0, 0, 0, 0 }\n"
        "}\n"
        "class \"GtkWidget\" style \"aireader\"\n"
        "widget_class \"*<GtkButton>*\" style \"aireader-button\"\n"
        "widget \"*.aireader-key\" style \"aireader-key\"\n";
    gtk_rc_parse_string(rc.c_str());
}

void styleButton(GtkWidget* button) {
    g_signal_connect(button, "expose-event", G_CALLBACK(buttonExpose), nullptr);
}

void setSelected(GtkWidget* button, bool selected) {
    if (isSelected(button) == selected) return;
    g_object_set_data(G_OBJECT(button), selectedKey, GINT_TO_POINTER(selected));
    GdkColor white = {0, 0xffff, 0xffff, 0xffff};
    tintLabels(button, selected ? &white : nullptr);
    gtk_widget_queue_draw(button);
}

void styleKey(GtkWidget* button) {
    gtk_widget_set_name(button, "aireader-key");
}

void styleEntry(GtkWidget* entry) {
    g_signal_connect(entry, "expose-event", G_CALLBACK(entryExpose), nullptr);
}

void styleCheck(GtkWidget* toggle) {
    g_signal_connect(toggle, "expose-event", G_CALLBACK(checkExpose), nullptr);
}

void styleScrollbar(GtkWidget* scrollbar) {
    g_signal_connect(scrollbar, "expose-event", G_CALLBACK(scrollbarExpose), nullptr);
}

void styleSeparator(GtkWidget* separator) {
    g_signal_connect(separator, "expose-event", G_CALLBACK(separatorExpose), nullptr);
}

}  // namespace Theme
