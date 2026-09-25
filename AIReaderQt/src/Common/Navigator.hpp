#pragma once

#include <QStackedWidget>

#include <string>

class Screen;

/// The window's stack of screens, the way the Kindle app has one: a screen
/// opens over the one before and closing it goes back. Escape goes back too.
class Navigator : public QStackedWidget {
public:
    Navigator();

    void push(Screen* screen);
    /// Closes the screen on top. It is deleted once the current event is
    /// done, so a screen may close itself from its own button.
    void pop();
    /// Closes screens until the one tagged `tag` is on top.
    void popTo(const std::string& tag);
    void popToRoot();

private:
    Screen* top() const;
    void removeTop();
    void returned();
};
