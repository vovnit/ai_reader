#pragma once

#include "../../Services/Env.hpp"

class Navigator;

namespace WordsView {

/// `bookId` 0 lists the words of every book.
void open(Env& env, Navigator& navigator, long long bookId);

}  // namespace WordsView
