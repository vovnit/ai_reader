#include "App/SmokeScript.hpp"

#include "Common/Tappable.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QTest>
#include <QTextStream>
#include <QTimer>
#include <QWindow>

#include <cstdio>
#include <memory>

namespace SmokeScript {

namespace {

struct Runner {
    QWidget* window;
    QStringList lines;
    int next = 0;
};

/// What is in front: an open menu, then a dialog, then the window.
QWidget* front(QWidget* window) {
    if (QWidget* popup = QApplication::activePopupWidget()) return popup;
    if (QWidget* modal = QApplication::activeModalWidget()) return modal;
    return window;
}

QString plain(QString text) {
    return text.remove('&').trimmed();
}

bool click(QWidget* window, const QString& text) {
    QWidget* top = front(window);
    if (auto* menu = qobject_cast<QMenu*>(top)) {
        for (QAction* action : menu->actions()) {
            if (plain(action->text()) != text) continue;
            menu->hide();
            action->trigger();
            return true;
        }
        return false;
    }
    // A button saying that, else one starting with it: the reader's footer
    // changes with every page.
    for (bool exact : {true, false}) {
        for (QAbstractButton* button : top->findChildren<QAbstractButton*>()) {
            if (!button->isVisible() || !button->isEnabled()) continue;
            QString label = plain(button->text());
            if (exact ? label != text : !label.startsWith(text)) continue;
            button->click();
            return true;
        }
    }
    // A row: the first clickable one with a label saying that.
    for (Tappable* row : top->findChildren<Tappable*>()) {
        if (!row->isVisible()) continue;
        for (QLabel* label : row->findChildren<QLabel*>()) {
            if (!label->text().contains(text)) continue;
            QPoint centre = row->rect().center();
            QTest::mouseClick(row, Qt::LeftButton, {}, centre);
            return true;
        }
    }
    return false;
}

Qt::Key keyNamed(const QString& name) {
    if (name == "Return") return Qt::Key_Return;
    if (name == "Escape") return Qt::Key_Escape;
    if (name == "Left") return Qt::Key_Left;
    if (name == "Right") return Qt::Key_Right;
    if (name == "PageDown") return Qt::Key_PageDown;
    if (name == "PageUp") return Qt::Key_PageUp;
    return Qt::Key_Space;
}

void step(const std::shared_ptr<Runner>& runner) {
    while (runner->next < runner->lines.size()) {
        QString line = runner->lines[runner->next++].trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        QString command = line.section(' ', 0, 0);
        QString rest = line.section(' ', 1);
        std::printf("script: %s\n", qPrintable(line));
        std::fflush(stdout);

        // The next command is due before this one runs: a click that opens a
        // menu or a dialog does not return until it closes, and the command
        // after it is the one that closes it.
        int delay = command == "wait" ? rest.toInt() : 150;
        QTimer::singleShot(delay, [runner] { step(runner); });

        if (command == "click") {
            if (!click(runner->window, rest)) std::printf("script: nothing to click saying “%s”\n", qPrintable(rest));
        } else if (command == "tap") {
            auto* page = runner->window->findChild<QWidget*>("page");
            if (page && page->isVisible()) {
                QPoint point(rest.section(' ', 0, 0).toInt(), rest.section(' ', 1, 1).toInt());
                QTest::mouseClick(page, Qt::LeftButton, {}, point);
            } else {
                std::printf("script: no page to tap\n");
            }
        } else if (command == "type") {
            // Key events carrying the text, since the test helpers know only
            // ASCII and a book's words seldom are.
            if (QWidget* focus = QApplication::focusWidget()) {
                for (QChar character : rest) {
                    QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(character));
                    QApplication::sendEvent(focus, &press);
                }
            }
        } else if (command == "key") {
            // Drawn offscreen, the window is not given back the focus when a
            // dialog closes, and its shortcuts wait for it.
            if (front(runner->window) == runner->window && !QApplication::activeWindow()) {
                runner->window->windowHandle()->requestActivate();
                QApplication::processEvents();
            }
            QWidget* target = QApplication::focusWidget() ? QApplication::focusWidget() : front(runner->window);
            QTest::keyClick(target, keyNamed(rest));
        } else if (command == "snap") {
            front(runner->window)->grab().save(rest);
        } else if (command == "quit") {
            // Out of any menu or dialog first, or quitting only ends theirs.
            if (QWidget* popup = QApplication::activePopupWidget()) popup->close();
            if (QWidget* modal = QApplication::activeModalWidget()) modal->close();
            QApplication::exit(0);
        }
        std::fflush(stdout);
        return;
    }
}

}  // namespace

void start(QWidget* window) {
    QByteArray path = qgetenv("AIREADER_SCRIPT");
    if (path.isEmpty()) return;
    QFile file(QString::fromLocal8Bit(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::fprintf(stderr, "script: cannot read %s\n", path.constData());
        return;
    }
    auto runner = std::make_shared<Runner>();
    runner->window = window;
    runner->lines = QTextStream(&file).readAll().split('\n');
    QTimer::singleShot(500, [runner] { step(runner); });
}

}  // namespace SmokeScript
