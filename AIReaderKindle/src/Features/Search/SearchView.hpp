#pragma once

#include "../../Services/Env.hpp"
#include "../Reader/ReaderLink.hpp"

#include <string>

class Navigator;

/// Searching the book — or its group — for a phrase. Tapping a hit turns
/// the reader to it.
namespace SearchView {

/// `covers` names what is searched, e.g. the book or the group.
void open(Navigator& navigator, const ReaderLink& link, const std::string& covers);

}  // namespace SearchView
