#pragma once

#include "../../Services/Env.hpp"
#include "../Reader/ReaderLink.hpp"

#include <string>

class Navigator;

/// What the book says a name or a word is, and the passages it was read
/// from; tapping a passage turns the reader to it.
namespace XRayView {

void open(Env& env, Navigator& navigator, const std::string& term, const ReaderLink& link);

/// Asks for a name or a word to X-ray, then opens it.
void ask(Env& env, Navigator& navigator, const ReaderLink& link);

}  // namespace XRayView
