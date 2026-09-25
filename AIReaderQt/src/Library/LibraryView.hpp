#pragma once

#include "Common/Screen.hpp"
#include "Features/Library/LibraryFeature.hpp"

class QVBoxLayout;

/// The root screen: the books on the shelf, grouped, and the way to
/// settings and the words met so far.
class LibraryView : public Screen {
public:
    LibraryView(Env& env, Navigator& navigator);

    /// The folders may have changed while a book was open, and a book just
    /// closed has a place worth sending on.
    void returned() override;

private:
    Env& env_;
    LibraryFeature feature_;
    QVBoxLayout* list_;

    void render();
    QWidget* row(const Book& book);
    QWidget* heading(const BookGroup& group, size_t count);
    void pickGroup(const Book& book, QWidget* anchor);
    void addBook();
};
