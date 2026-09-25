#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>

/// One window, a stack of screens: the Kindle's window manager is happiest
/// with a single application window, and a stack is how the iOS app reads
/// too. Popping destroys the screen, which frees whatever it owned.
class Navigator {
public:
    Navigator();

    GtkWidget* widget() const { return notebook_; }
    GtkWindow* window() const;

    void push(GtkWidget* screen);
    void pop();
    void popToRoot();
    /// Pops until the screen tagged so is on top; nothing if there is none.
    void popTo(const std::string& tag);
    int depth() const;

    /// Names a screen so it can be returned to with `popTo`.
    static void tag(GtkWidget* screen, const std::string& tag);

    /// Called on a screen when it comes back to the front after a pop.
    static void onReturn(GtkWidget* screen, std::function<void()> callback);

private:
    GtkWidget* notebook_;

    void showTop();
};
