#pragma once

#include "../../Services/Env.hpp"

class Navigator;
class ReaderFeature;

/// A book's offline glossary: how many of its words have a definition, what
/// defining the rest should cost, and the button that does it.
namespace GlossaryView {

void open(Env& env, Navigator& navigator, ReaderFeature& reader);

}  // namespace GlossaryView
