#pragma once

#include "Common/Screen.hpp"
#include "Features/Reader/ReaderFeature.hpp"
#include "Features/Reader/ReaderLink.hpp"

class PageView;
class QPushButton;

/// The book as pages. Clicking a word looks it up; clicking blank space,
/// the arrow keys or the wheel turn the page; the footer opens the menu.
class ReaderView : public Screen {
public:
    ReaderView(Env& env, Navigator& navigator, const Book& book);

    /// Display changes made from the menu land here.
    void returned() override;

    /// What the reader's screens — lookups, and what the menu opens — carry
    /// back to it.
    ReaderLink link();

private:
    Env& env_;
    ReaderFeature feature_;
    PageView* page_;
    QPushButton* progress_;

    void render();
    void tapped(const ReaderFeature::Tap& tap);
    void showMenu();
    /// What a search from here runs over, in words.
    std::string covers() const;
};
