#pragma once

#include "../../Services/Env.hpp"
#include "../Reader/ReaderLink.hpp"

class Navigator;
class ReaderFeature;

/// The reader's menu: the table of contents, what has been looked up in this
/// book, a search of it, an X-ray of a name, a conversation about the page on
/// screen, how the text is rendered, and the way out.
namespace MenuView {

void open(Env& env, Navigator& navigator, ReaderFeature& reader, const ReaderLink& link);

}  // namespace MenuView
