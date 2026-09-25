#pragma once

#include "../../Domain/Books/Book.hpp"
#include "../../Domain/Search/BookSearch.hpp"

#include <gtk/gtk.h>

#include <functional>

/// One search hit as a row: the excerpt with the match in bold, and where it
/// is. Tappable when given somewhere to go.
namespace HitView {

GtkWidget* create(const SearchHit& hit, bool showBook, std::function<void(const BookPosition&)> onTap);

}  // namespace HitView
