#include "Paginator.hpp"

PageLayout::PageLayout(PageLayout&& other) noexcept
    : layout(other.layout), pages(std::move(other.pages)), pictures(std::move(other.pictures)) {
    other.layout = nullptr;
}

PageLayout& PageLayout::operator=(PageLayout&& other) noexcept {
    if (this != &other) {
        if (layout) g_object_unref(layout);
        layout = other.layout;
        pages = std::move(other.pages);
        pictures = std::move(other.pictures);
        other.layout = nullptr;
    }
    return *this;
}

PageLayout::~PageLayout() {
    if (layout) g_object_unref(layout);
}

int PageLayout::pageContaining(int offset) const {
    for (size_t i = 0; i < pages.size(); ++i) {
        if (offset >= pages[i].start && offset < pages[i].end) return static_cast<int>(i);
    }
    return pages.empty() ? 0 : static_cast<int>(pages.size()) - 1;
}

namespace Paginator {

/// `fontPixels` is the body size; a heading is set a step larger, a
/// superscript a step smaller and raised. Each is given
/// as an absolute size of its own rather than a scale: the Kindle's Pango
/// 1.26 applies a scale attribute through `set_size`, which reads the pixel
/// size as points and blows a heading up several times over.
static PangoAttrList* attributes(const PlainText& chapter, double fontPixels) {
    const double headingPixels = 1.3 * fontPixels;
    const double smallPixels = 0.7 * fontPixels;
    PangoAttrList* list = pango_attr_list_new();
    for (const auto& span : chapter.spans) {
        auto add = [&](PangoAttribute* attribute) {
            attribute->start_index = static_cast<guint>(span.start);
            attribute->end_index = static_cast<guint>(span.end);
            pango_attr_list_insert(list, attribute);
        };
        switch (span.kind) {
        case TextSpan::Kind::Bold:
            add(pango_attr_weight_new(PANGO_WEIGHT_BOLD));
            break;
        case TextSpan::Kind::Italic:
            add(pango_attr_style_new(PANGO_STYLE_ITALIC));
            break;
        case TextSpan::Kind::Heading:
            add(pango_attr_weight_new(PANGO_WEIGHT_BOLD));
            add(pango_attr_size_new_absolute(static_cast<int>(headingPixels * PANGO_SCALE)));
            break;
        case TextSpan::Kind::Superscript:
            add(pango_attr_size_new_absolute(static_cast<int>(smallPixels * PANGO_SCALE)));
            add(pango_attr_rise_new(static_cast<int>(0.35 * fontPixels * PANGO_SCALE)));
            break;
        case TextSpan::Kind::Subscript:
            add(pango_attr_size_new_absolute(static_cast<int>(smallPixels * PANGO_SCALE)));
            add(pango_attr_rise_new(static_cast<int>(-0.15 * fontPixels * PANGO_SCALE)));
            break;
        }
    }
    return list;
}

PageLayout paginate(
    PangoContext* context,
    const PlainText& chapter,
    const ReadingStyle& style,
    int width,
    int height,
    double scale,
    const std::string& language)
{
    PageLayout result;
    if (width < 1 || height < 1) return result;

    PangoLayout* layout = pango_layout_new(context);
    result.layout = layout;

    PangoFontDescription* font = pango_font_description_new();
    pango_font_description_set_family(font, style.fontName.empty() ? "Serif" : style.fontName.c_str());
    double fontPixels = ReadingStyle::basePixelSize * style.scale * scale;
    pango_font_description_set_absolute_size(font, fontPixels * PANGO_SCALE);
    pango_layout_set_font_description(layout, font);
    pango_font_description_free(font);

    int indent = static_cast<int>(1.5 * fontPixels);
    pango_layout_set_width(layout, width * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_justify(layout, TRUE);
    pango_layout_set_spacing(layout, static_cast<int>(style.lineSpacing * scale * PANGO_SCALE));
    // A first-line indent marks paragraphs without spending a blank line on each.
    pango_layout_set_indent(layout, indent * PANGO_SCALE);
    PangoAttrList* list = attributes(chapter, fontPixels);
    // A picture sits on a line of its own, indented like any first line.
    Illustrations::attach(list, chapter, std::max(1, width - indent), height, result.pictures);
    if (!language.empty()) {
        pango_attr_list_insert(list, pango_attr_language_new(pango_language_from_string(language.c_str())));
    }
    pango_layout_set_attributes(layout, list);
    pango_attr_list_unref(list);
    pango_layout_set_text(layout, chapter.text.c_str(), static_cast<int>(chapter.text.size()));

    const int pageHeight = height * PANGO_SCALE;
    const int length = static_cast<int>(chapter.text.size());
    PangoLayoutIter* iter = pango_layout_get_iter(layout);
    Page current;
    bool started = false;
    do {
        PangoRectangle logical;
        pango_layout_iter_get_line_extents(iter, nullptr, &logical);
        PangoLayoutLine* line = pango_layout_iter_get_line_readonly(iter);
        int bottom = logical.y + logical.height;

        if (!started) {
            current = Page{line->start_index, length, logical.y, 0};
            started = true;
        } else if (bottom - current.top > pageHeight) {
            // This line does not fit: close the page above it and open a new one.
            current.end = line->start_index;
            current.height = logical.y - current.top;
            result.pages.push_back(current);
            current = Page{line->start_index, length, logical.y, 0};
        }
        current.height = bottom - current.top;
    } while (pango_layout_iter_next_line(iter));
    pango_layout_iter_free(iter);

    current.end = length;
    result.pages.push_back(current);
    return result;
}

}  // namespace Paginator
