#pragma once

#include "Common/Screen.hpp"
#include "Features/Reader/ReaderFeature.hpp"
#include "Features/Reader/ReaderLink.hpp"

/// The book's table of contents, opened at the entry being read.
class ContentsView : public Screen {
public:
    ContentsView(Navigator& navigator, const ReaderFeature& reader, const ReaderLink& link);
};
