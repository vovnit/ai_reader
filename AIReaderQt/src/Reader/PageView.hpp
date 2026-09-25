#pragma once

#include "Features/Reader/ReaderFeature.hpp"

#include <QImage>
#include <QWidget>

#include <functional>

/// The page itself: the Kindle app's Pango layout of the chapter, drawn
/// through cairo into an image at the screen's own density. Clicks go to
/// the reader as taps; arrow keys and the wheel turn pages.
class PageView : public QWidget {
public:
    explicit PageView(ReaderFeature& feature);
    ~PageView() override;

    /// Draws the page on screen again, after the reader changed.
    void refresh();

    std::function<void(const ReaderFeature::Tap&)> onTap;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    ReaderFeature& feature_;
    PangoContext* context_;
    QImage page_;
    int wheel_ = 0;

    QImage render() const;
};
