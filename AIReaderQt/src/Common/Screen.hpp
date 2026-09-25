#pragma once

#include <QWidget>

#include <functional>
#include <string>

class Navigator;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;

/// One screen on the navigator: a header — the way back, the title, the
/// screen's own actions — over its body.
class Screen : public QWidget {
public:
    /// `back` labels the button that closes the screen; the root screen,
    /// which has nothing to go back to, passes an empty one.
    Screen(Navigator& navigator, const QString& title, const QString& back = "Back");

    /// The screen owns the body.
    void setBody(QWidget* body);
    void setTitle(const QString& title);
    QPushButton* addAction(const QString& label, std::function<void()> action);

    /// When the screen comes back to the front, the one above it closed.
    virtual void returned() {}

    /// The name `Navigator::popTo` finds the screen by.
    std::string tag;

protected:
    Navigator& navigator;

private:
    QVBoxLayout* layout_;
    QHBoxLayout* header_;
    QLabel* title_;
};
