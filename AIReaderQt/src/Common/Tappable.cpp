#include "Common/Tappable.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyle>

Tappable::Tappable(std::function<void()> action) : action_(std::move(action)) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
    setProperty("selected", false);
    setStyleSheet(
        "Tappable { border-radius: 6px; }"
        "Tappable[card=\"true\"] { border: 1px solid palette(mid); }"
        "Tappable:hover, Tappable:focus { background: palette(alternate-base); }"
        "Tappable[selected=\"true\"] { background: palette(highlight); }"
        "Tappable[selected=\"true\"] QLabel { color: palette(highlighted-text); }");
}

void Tappable::setSelected(bool selected) {
    if (property("selected").toBool() == selected) return;
    setProperty("selected", selected);
    // A dynamic property only shows once the style is applied again.
    style()->unpolish(this);
    style()->polish(this);
    for (QWidget* child : findChildren<QWidget*>()) {
        style()->unpolish(child);
        style()->polish(child);
    }
}

void Tappable::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) && action_) action_();
}

void Tappable::keyPressEvent(QKeyEvent* event) {
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) && action_) {
        action_();
        return;
    }
    QFrame::keyPressEvent(event);
}
