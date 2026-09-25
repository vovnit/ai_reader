#pragma once

#include "../../Services/Env.hpp"

class Navigator;

/// The root screen: the books on the shelf, and the way to settings and words.
namespace LibraryView {

void open(Env& env, Navigator& navigator);

}  // namespace LibraryView
