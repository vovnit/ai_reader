#pragma once

#include "../../Domain/Books/Book.hpp"

class LibraryFeature;
class Navigator;

/// Choosing a group for a book: one of the groups there are, none, or a new
/// one typed in.
namespace GroupView {

void pick(Navigator& navigator, LibraryFeature& feature, const Book& book);

}  // namespace GroupView
