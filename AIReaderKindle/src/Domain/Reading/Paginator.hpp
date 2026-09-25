#pragma once

#include "../Books/HtmlText.hpp"
#include "Illustrations.hpp"
#include "ReadingStyle.hpp"

#include <pango/pango.h>

#include <string>
#include <vector>

/// One page: a byte range of the chapter and where it sits in the layout.
/// Positions are in Pango units.
struct Page {
    int start = 0;
    int end = 0;
    int top = 0;
    int height = 0;
};

/// A chapter laid out once at a given width, split into pages of whole lines.
/// A page is drawn by translating the layout so the page's top lines up with
/// the page's top, which keeps line breaks identical across pages.
class PageLayout {
public:
    PageLayout() = default;
    PageLayout(PageLayout&& other) noexcept;
    PageLayout& operator=(PageLayout&& other) noexcept;
    PageLayout(const PageLayout&) = delete;
    PageLayout& operator=(const PageLayout&) = delete;
    ~PageLayout();

    PangoLayout* layout = nullptr;
    std::vector<Page> pages;
    /// The chapter's pictures, referenced by the layout's shape attributes.
    std::vector<std::unique_ptr<Picture>> pictures;

    bool isEmpty() const { return pages.empty(); }
    int pageContaining(int offset) const;
};

namespace Paginator {

/// Lays a chapter out for a page of `width` × `height` pixels. `scale` is
/// how much denser than a desktop the screen is; sizes in the style are
/// multiplied by it.
PageLayout paginate(
    PangoContext* context,
    const PlainText& chapter,
    const ReadingStyle& style,
    int width,
    int height,
    double scale,
    const std::string& language);

}  // namespace Paginator
