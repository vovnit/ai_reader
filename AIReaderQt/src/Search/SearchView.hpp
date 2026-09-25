#pragma once

#include "Common/Screen.hpp"
#include "Features/Reader/ReaderLink.hpp"
#include "Features/Search/SearchFeature.hpp"

class QLineEdit;
class QPushButton;
class QVBoxLayout;

/// Finding a phrase in the book and the rest of its group; a hit opens the
/// reader there.
class SearchView : public Screen {
public:
    /// `covers` says in words what is searched: “this book”, or the group.
    SearchView(Navigator& navigator, const ReaderLink& link, std::string covers);

private:
    SearchFeature feature_;
    ReaderLink link_;
    std::string covers_;
    QLineEdit* query_;
    QPushButton* go_;
    QVBoxLayout* list_;

    void render();
};
