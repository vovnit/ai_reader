#pragma once

#include <gtk/gtk.h>

/// A scripted walk through the interface, for looking at the app where no
/// finger can reach it. Enabled by `AIREADER_SCRIPT=<file>`, one command per
/// line:
///
///   wait <ms>        pause
///   tap <x> <y>      press at window coordinates: a button is clicked,
///                    anything else receives a button-press event
///   type <text>      put text into the focused entry
///   dump             print the visible widgets with their positions
///   snap <file.png>  save what the window shows (not on every backend)
///   quit
namespace SmokeScript {

void start(GtkWidget* window);

}  // namespace SmokeScript
