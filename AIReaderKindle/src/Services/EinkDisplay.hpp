#pragma once

#include <cairo.h>

/// The e-ink panel under the Kindle's X server. The window manager refreshes
/// whatever X draws; this puts a page on the panel itself, with the slide
/// the Kindle's own reader turns pages with — an animation the display
/// driver of the MediaTek Kindles (PaperWhite 5 and later) plays in
/// hardware, as KOReader asks for it. Elsewhere it does nothing.
namespace EinkDisplay {

enum class Slide { Left, Right };

/// Shows `page`, an image surface, at (`x`, `y`) of the screen, the new
/// content sliding in. Returns false when this screen cannot, in which case
/// nothing was drawn and the caller shows the page the ordinary way. The
/// caller draws it that way afterwards regardless, so X agrees with the panel.
bool slide(cairo_surface_t* page, int x, int y, Slide direction);

}  // namespace EinkDisplay
