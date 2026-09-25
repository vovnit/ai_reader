#pragma once

#include <gtk/gtk.h>

#include <string>

/// How the controls look. GTK+ 2 draws every control as a desktop of the
/// nineties did — bevelled buttons, sunken fields, arrows on scrollbars —
/// so each one is drawn here instead: black on white, thin rounded outlines,
/// nothing raised or sunken. That is what an e-ink screen shows well and
/// what the Kindle's own screens look like.
namespace Theme {

/// Colours, fonts and spacing, through an rc style. `font` is a Pango
/// description for the interface text; a size in pixels sets the small and
/// big text off it too.
void apply(const std::string& font);

/// A rounded outline, filled black with white text while pressed. A button
/// with no relief has no outline, only the fill while pressed.
void styleButton(GtkWidget* button);
/// Keeps a button filled, as while pressed, until told otherwise: the card
/// the reader has chosen and not yet paired.
void setSelected(GtkWidget* button, bool selected);
/// Less room around the label than a button has: for keyboard keys.
void styleKey(GtkWidget* button);
/// A rounded outline, heavier while the entry has the focus.
void styleEntry(GtkWidget* entry);
/// A square with a tick in it when the toggle is active. Drawn on a
/// toggle button with no child.
void styleCheck(GtkWidget* toggle);
/// A thin bar and no arrows; nothing at all when there is nothing to scroll.
void styleScrollbar(GtkWidget* scrollbar);
/// A single light line.
void styleSeparator(GtkWidget* separator);

}  // namespace Theme
