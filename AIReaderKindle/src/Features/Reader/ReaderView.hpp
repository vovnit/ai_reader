#pragma once

#include "../../Domain/Books/Book.hpp"
#include "../../Services/Env.hpp"
#include "ReaderLink.hpp"

class Navigator;
class ReaderFeature;

/// The book as pages. Tapping a word looks it up; tapping blank space turns
/// the page; the footer opens the menu.
namespace ReaderView {

void open(Env& env, Navigator& navigator, const Book& book);

/// The link the reader's screens — lookups, the menu and what it opens —
/// carry back to it.
ReaderLink link(Env& env, Navigator& navigator, ReaderFeature& reader);

}  // namespace ReaderView
