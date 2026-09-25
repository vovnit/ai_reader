#pragma once

class QWidget;

/// A scripted walk through the interface, for looking at the app where no
/// one is clicking — in a container, drawn offscreen. Enabled by
/// `AIREADER_SCRIPT=<file>`, one command per line:
///
///   wait <ms>        pause
///   click <text>     click the button, menu entry or row showing that text
///                    (in a dialog or menu when one is open)
///   tap <x> <y>      click the page at that point
///   type <text>      type into the focused field
///   key <name>       press Return, Escape, Left, Right, PageDown…
///   snap <file.png>  save what the window, or the dialog on top, shows
///   quit
namespace SmokeScript {

void start(QWidget* window);

}  // namespace SmokeScript
