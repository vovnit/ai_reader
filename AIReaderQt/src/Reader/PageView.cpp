#include "Reader/PageView.hpp"

#include "Domain/Reading/Illustrations.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <pango/pangocairo.h>

PageView::PageView(ReaderFeature& feature) : feature_(feature) {
    setObjectName("page");
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(240, 240);
    // Sizes in the layout are absolute pixels, so the resolution only has to
    // be something sensible.
    context_ = pango_font_map_create_context(pango_cairo_font_map_get_default());
    pango_cairo_context_set_resolution(context_, 96);
    Illustrations::install(context_);
}

PageView::~PageView() {
    g_object_unref(context_);
}

void PageView::refresh() {
    page_ = render();
    update();
}

/// The page as a picture the widget's size in device pixels, or nothing when
/// there is no page to show.
QImage PageView::render() const {
    const Page* page = feature_.page();
    qreal ratio = devicePixelRatioF();
    int width = qRound(this->width() * ratio);
    int height = qRound(this->height() * ratio);
    if (feature_.status() != ReaderFeature::Status::Loaded || !page || !feature_.layout().layout || width < 1 || height < 1) {
        return {};
    }
    // Cairo's ARGB32 is Qt's premultiplied ARGB32, byte for byte.
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        image.bits(), CAIRO_FORMAT_ARGB32, width, height, static_cast<int>(image.bytesPerLine()));
    cairo_t* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 0, 0, 0);
    int margin = feature_.marginPixels();
    // The whole chapter is laid out once; a page is the slice of it that
    // starts at the page's top.
    cairo_rectangle(cr, margin, margin, width - 2 * margin, page->height / PANGO_SCALE + 1);
    cairo_clip(cr);
    cairo_translate(cr, margin, margin - page->top / static_cast<double>(PANGO_SCALE));
    pango_cairo_show_layout(cr, feature_.layout().layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    image.setDevicePixelRatio(ratio);
    return image;
}

void PageView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    if (!page_.isNull()) {
        painter.drawImage(0, 0, page_);
        return;
    }
    QString text;
    if (feature_.status() == ReaderFeature::Status::Loading) text = "Opening…";
    if (feature_.status() == ReaderFeature::Status::Failed) {
        text = "Couldn’t open the book\n" + QString::fromStdString(feature_.error());
    }
    painter.setPen(Qt::black);
    painter.drawText(rect().adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap, text);
}

void PageView::resizeEvent(QResizeEvent*) {
    qreal ratio = devicePixelRatioF();
    // The layout is made in device pixels: the style's sizes, meant for a
    // plain desktop screen, are multiplied by the density.
    feature_.setContext(context_, ratio);
    feature_.setPageSize(qRound(width() * ratio), qRound(height() * ratio));
    refresh();
}

void PageView::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    qreal ratio = devicePixelRatioF();
    ReaderFeature::Tap tap = feature_.tapAt(event->position().x() * ratio, event->position().y() * ratio);
    if (onTap) onTap(tap);
}

void PageView::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_PageUp:
    case Qt::Key_Backspace:
        feature_.previous();
        break;
    case Qt::Key_Right:
    case Qt::Key_PageDown:
    case Qt::Key_Space:
        feature_.next();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void PageView::wheelEvent(QWheelEvent* event) {
    // A notch of the wheel is a page; a touchpad's small steps add up to one.
    wheel_ += event->angleDelta().y();
    if (wheel_ >= 120) {
        wheel_ = 0;
        feature_.previous();
    } else if (wheel_ <= -120) {
        wheel_ = 0;
        feature_.next();
    }
}
