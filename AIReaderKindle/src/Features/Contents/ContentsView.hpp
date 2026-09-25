#pragma once

#include "../Reader/ReaderLink.hpp"

class Navigator;
class ReaderFeature;

/// The book's table of contents: one row per entry, nested ones indented,
/// the one being read marked. A tap turns the reader to it.
namespace ContentsView {

void open(Navigator& navigator, ReaderFeature& reader, const ReaderLink& link);

}  // namespace ContentsView
