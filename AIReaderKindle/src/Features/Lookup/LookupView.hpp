#pragma once

#include "../../Domain/AI/WordExplanation.hpp"
#include "../../Services/Env.hpp"
#include "../Reader/ReaderLink.hpp"

class Navigator;

/// The explanation screen for a tapped word: what it means here and what its
/// dictionary form is — with the way on to the dictionary's own entry, to
/// what the book says about the word, and to a conversation about it.
namespace LookupView {

/// `link` is empty when no book is open, as from the word list.
void open(Env& env, Navigator& navigator, const LookupContext& context, const ReaderLink& link);

}  // namespace LookupView
