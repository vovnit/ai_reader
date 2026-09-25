#pragma once

#include <QFrame>

#include <functional>

/// A row or a card that does something when clicked — a book, a hit, a
/// word to pair — holding whatever labels describe it.
class Tappable : public QFrame {
    // So style sheets can name the class.
    Q_OBJECT

public:
    explicit Tappable(std::function<void()> action);

    /// Drawn highlighted, as a picked card is.
    void setSelected(bool selected);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    std::function<void()> action_;
};
