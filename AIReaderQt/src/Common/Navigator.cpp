#include "Common/Navigator.hpp"

#include "Common/Screen.hpp"

#include <QKeySequence>
#include <QShortcut>

Navigator::Navigator() {
    auto* back = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    QObject::connect(back, &QShortcut::activated, this, [this] {
        if (count() > 1) pop();
    });
}

void Navigator::push(Screen* screen) {
    addWidget(screen);
    setCurrentWidget(screen);
    screen->setFocus();
}

void Navigator::pop() {
    if (count() < 2) return;
    removeTop();
    returned();
}

void Navigator::popTo(const std::string& tag) {
    while (count() > 1 && top()->tag != tag) removeTop();
    returned();
}

void Navigator::popToRoot() {
    while (count() > 1) removeTop();
    returned();
}

Screen* Navigator::top() const {
    return static_cast<Screen*>(widget(count() - 1));
}

void Navigator::removeTop() {
    Screen* screen = top();
    removeWidget(screen);
    screen->hide();
    screen->deleteLater();
}

void Navigator::returned() {
    Screen* screen = top();
    setCurrentWidget(screen);
    screen->setFocus();
    screen->returned();
}
