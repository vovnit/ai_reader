#pragma once

#include "Domain/Books/Book.hpp"
#include "Domain/Search/BookSearch.hpp"

#include <functional>

class QWidget;

/// One place a phrase was found: the excerpt with the match in bold and
/// where it is. Clickable when `onTap` is set.
namespace HitView {

QWidget* create(const SearchHit& hit, bool showBook, std::function<void(const BookPosition&)> onTap);

}  // namespace HitView
