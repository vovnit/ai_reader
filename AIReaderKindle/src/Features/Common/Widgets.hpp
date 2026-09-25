#pragma once

#include <gtk/gtk.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class Navigator;

/// The handful of GTK conveniences the views share: closures on signals,
/// screens with a header, labels that wrap, and Kindle-shaped dialogs.
namespace Widgets {

using Action = std::function<void()>;

/// Runs `action` on a signal whose only arguments are the widget and user data
/// ("clicked", "activate", "toggled", "changed").
void connect(GtkWidget* widget, const char* signal, Action action);

/// Runs `action` from the main loop, once the current signal handler is done.
void later(Action action);

/// Deletes `object` when `widget` is destroyed. A widget may own several
/// objects; the key comes from the object, so no two ever share one.
template <class T>
void own(GtkWidget* widget, T* object) {
    std::string key = "aireader-owned-" + std::to_string(reinterpret_cast<std::uintptr_t>(object));
    g_object_set_data_full(G_OBJECT(widget), key.c_str(), object,
                           [](gpointer pointer) { delete static_cast<T*>(pointer); });
}

/// How much bigger than a desktop window the screen is. Every size in the
/// interface is a desktop pixel count multiplied by this, so a Kindle's dense
/// screen gets controls a finger can hit.
double scale();
void setScale(double scale);
int px(int desktopPixels);

/// The interface text size in pixels: 15 desktop pixels scaled, unless the
/// font given to `Theme::apply` names a pixel size.
int fontPixels();
void setFontPixels(int pixels);
/// Markup a step smaller or larger than the interface text. Sized in
/// pixels rather than with <small> or <big>: the Kindle's Pango 1.26 reads
/// a scale as a point size and blows the text up several times over. Put
/// it around the other tags, not inside them; a size inside <b> or <i>
/// undoes them.
std::string small(const std::string& markup);
std::string big(const std::string& markup);

/// The width text may take before wrapping: the screen less the margins.
int wrapWidth();
void setWrapWidth(int width);

/// `action` may be empty when the handler needs the button itself; connect
/// it to "clicked" afterwards.
GtkWidget* button(const std::string& label, Action action);
/// A square button showing one symbol: ‹ › ✕ – +. Outlined unless `flat`.
GtkWidget* glyphButton(const std::string& glyph, Action action, bool flat = false);
/// An outlined button around `content` rather than a label: a card.
GtkWidget* contentButton(GtkWidget* content, Action action);
/// A button with no outline around `content`: a tappable row in a list.
GtkWidget* flatButton(GtkWidget* content, Action action);
/// A check box. `onToggle` gets the new state.
GtkWidget* check(bool active, std::function<void(bool)> onToggle);
GtkWidget* label(const std::string& text, double xalign = 0, bool wrap = true);
/// A label with Pango markup, escaped by the caller where needed.
GtkWidget* markup(const std::string& markup, double xalign = 0, bool wrap = true);
std::string escape(const std::string& text);
GtkWidget* scrolled(GtkWidget* child);
GtkWidget* separator();

/// A screen: a header with a back button, a title and optional actions above
/// a body. The body expands to fill the rest.
GtkWidget* screen(
    const std::string& title,
    GtkWidget* body,
    Action onBack,
    const std::string& backLabel = "Back",
    const std::vector<GtkWidget*>& actions = {});

/// A full-screen list of choices, one per tall button, the current one
/// marked. Drop-down menus need a mouse; this needs a finger.
void picker(
    Navigator& navigator,
    const std::string& title,
    const std::vector<std::string>& options,
    const std::string& selected,
    std::function<void(const std::string&)> onPick);

/// A text entry. Screens with entries carry a `Keyboard` for typing into them.
/// The frame is the theme's; a caller need not add one.
GtkWidget* entry(const std::string& text, bool secret = false);
std::string entryText(GtkWidget* entry);

/// Message dialogs. Neither blocks: `onYes` runs if the reader confirms.
void alert(GtkWindow* parent, const std::string& title, const std::string& message);
void confirm(GtkWindow* parent, const std::string& title, const std::string& message, const std::string& yes, Action onYes);

/// Sets the title the Kindle's window manager expects, e.g.
/// `L:A_N:application_ID:org.aireader.kindle_PC:N`. Plain elsewhere.
void kindleTitle(GtkWindow* window, const char* layer, const char* role, const char* extra);

}  // namespace Widgets
