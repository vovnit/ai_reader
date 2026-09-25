#include "Illustrations.hpp"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <cstring>

namespace {

struct Box {
    int maxWidth;
    int maxHeight;
};

/// Picks the decoded size before the pixels exist, so a large photograph is
/// never held at full size.
void sizePrepared(GdkPixbufLoader* loader, gint width, gint height, gpointer data) {
    const Box* box = static_cast<Box*>(data);
    if (width < 1 || height < 1) return;
    double scale = std::min({1.0, box->maxWidth / static_cast<double>(width), box->maxHeight / static_cast<double>(height)});
    gdk_pixbuf_loader_set_size(loader, std::max(1, static_cast<int>(width * scale)), std::max(1, static_cast<int>(height * scale)));
}

/// Copies a pixbuf into a Cairo surface, premultiplying alpha on the way.
cairo_surface_t* surfaceFrom(GdkPixbuf* pixbuf) {
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    int sourceStride = gdk_pixbuf_get_rowstride(pixbuf);
    const guchar* source = gdk_pixbuf_get_pixels(pixbuf);
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) return surface;
    cairo_surface_flush(surface);
    unsigned char* target = cairo_image_surface_get_data(surface);
    int targetStride = cairo_image_surface_get_stride(surface);
    for (int y = 0; y < height; ++y) {
        const guchar* in = source + y * sourceStride;
        uint32_t* out = reinterpret_cast<uint32_t*>(target + y * targetStride);
        for (int x = 0; x < width; ++x, in += channels) {
            uint32_t a = channels == 4 ? in[3] : 255;
            uint32_t r = in[0] * a / 255, g = in[1] * a / 255, b = in[2] * a / 255;
            out[x] = a << 24 | r << 16 | g << 8 | b;
        }
    }
    cairo_surface_mark_dirty(surface);
    return surface;
}

}  // namespace

std::unique_ptr<Picture> Picture::decode(const std::string& bytes, int maxWidth, int maxHeight) {
    if (bytes.empty() || maxWidth < 1 || maxHeight < 1) return nullptr;
    Box box{maxWidth, maxHeight};
    GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
    g_signal_connect(loader, "size-prepared", G_CALLBACK(sizePrepared), &box);
    bool ok = gdk_pixbuf_loader_write(loader, reinterpret_cast<const guchar*>(bytes.data()), bytes.size(), nullptr)
        && gdk_pixbuf_loader_close(loader, nullptr);
    GdkPixbuf* pixbuf = ok ? gdk_pixbuf_loader_get_pixbuf(loader) : nullptr;
    std::unique_ptr<Picture> picture;
    if (pixbuf) {
        cairo_surface_t* surface = surfaceFrom(pixbuf);
        picture.reset(new Picture(surface, gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf)));
    }
    g_object_unref(loader);
    return picture;
}

Picture::~Picture() {
    cairo_surface_destroy(surface_);
}

void Picture::draw(cairo_t* cr, double x, double y) const {
    cairo_save(cr);
    cairo_set_source_surface(cr, surface_, x, y);
    cairo_paint(cr);
    cairo_restore(cr);
}

namespace Illustrations {

void attach(PangoAttrList* list, const PlainText& chapter, int lineWidth, int pageHeight, std::vector<std::unique_ptr<Picture>>& pictures) {
    // A little under the page, so the line it sits on still fits.
    int maxHeight = static_cast<int>(pageHeight * 0.92);
    for (const auto& image : chapter.images) {
        auto picture = Picture::decode(image.bytes, lineWidth, maxHeight);
        if (!picture) continue;
        // The shape takes the whole line; the picture is centred in it when drawn.
        PangoRectangle logical{0, -picture->height() * PANGO_SCALE, lineWidth * PANGO_SCALE, picture->height() * PANGO_SCALE};
        PangoAttribute* shape = pango_attr_shape_new_with_data(&logical, &logical, picture.get(), nullptr, nullptr);
        shape->start_index = static_cast<guint>(image.offset);
        shape->end_index = static_cast<guint>(image.offset + std::strlen(imagePlaceholder));
        pango_attr_list_insert(list, shape);
        pictures.push_back(std::move(picture));
    }
}

static void render(cairo_t* cr, PangoAttrShape* shape, gboolean doPath, gpointer) {
    if (doPath || !shape->data) return;
    const Picture* picture = static_cast<const Picture*>(shape->data);
    double x = 0, y = 0;
    cairo_get_current_point(cr, &x, &y);
    double slot = shape->logical_rect.width / static_cast<double>(PANGO_SCALE);
    picture->draw(cr, x + (slot - picture->width()) / 2, y + shape->logical_rect.y / static_cast<double>(PANGO_SCALE));
}

void install(PangoContext* context) {
    pango_cairo_context_set_shape_renderer(context, render, nullptr, nullptr);
}

}  // namespace Illustrations
