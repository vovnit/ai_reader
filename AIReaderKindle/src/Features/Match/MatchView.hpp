#pragma once

#include "../../Services/Env.hpp"

class Navigator;

/// The matching game: words on the left, meanings on the right, tap one of
/// each.
namespace MatchView {

/// `bookId` 0 plays with the words of every book.
void open(Env& env, Navigator& navigator, long long bookId);

}  // namespace MatchView
