#pragma once

#include <gtk/gtk.h>

/// An on-screen keyboard drawn by the app: the Kindle's own keyboard reaches a
/// GTK window only as X key events, which carry Latin and nothing else. This
/// one types into whichever entry has focus, in Latin with French accents, in
/// Cyrillic, or in digits and punctuation.
namespace Keyboard {

/// A keyboard for the entries in `toplevel`'s window. Sits where it is packed.
GtkWidget* create(GtkWidget* toplevel);

}  // namespace Keyboard
