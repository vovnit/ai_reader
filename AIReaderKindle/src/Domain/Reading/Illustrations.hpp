#pragma once

#include "../Books/HtmlText.hpp"

#include <cairo.h>
#include <pango/pango.h>

#include <memory>
#include <string>
#include <vector>

/// A decoded illustration, already scaled to fit a page, ready to draw.
class Picture {
public:
    /// Decodes an image, shrinking it to fit within the box. Nothing if the
    /// bytes are not a picture the platform can read.
    static std::unique_ptr<Picture> decode(const std::string& bytes, int maxWidth, int maxHeight);
    ~Picture();
    Picture(const Picture&) = delete;
    Picture& operator=(const Picture&) = delete;

    int width() const { return width_; }
    int height() const { return height_; }
    void draw(cairo_t* cr, double x, double y) const;

private:
    Picture(cairo_surface_t* surface, int width, int height)
        : surface_(surface), width_(width), height_(height) {}

    cairo_surface_t* surface_;
    int width_;
    int height_;
};

/// Pictures in the flow of the text: each placeholder character in a chapter
/// becomes a Pango shape the size of its picture, so pagination treats it as
/// a tall line, and a renderer on the context draws the picture there.
namespace Illustrations {

/// Adds one shape attribute per picture. The pictures are kept in `pictures`
/// and must outlive any layout using the list.
void attach(
    PangoAttrList* list,
    const PlainText& chapter,
    int lineWidth,
    int pageHeight,
    std::vector<std::unique_ptr<Picture>>& pictures);

/// Makes layouts on this context draw the pictures their shapes stand for.
void install(PangoContext* context);

}  // namespace Illustrations
