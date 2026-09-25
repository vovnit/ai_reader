#pragma once

#include "Common/Screen.hpp"
#include "Features/Lookup/LookupFeature.hpp"
#include "Features/Reader/ReaderLink.hpp"

class QVBoxLayout;

/// A clicked word explained: what it means here and its dictionary form,
/// with the way on to the dictionary's own entry, to what the book says
/// about it, and to a conversation about it.
class LookupView : public Screen {
public:
    /// `link` is empty when no book is open, as from the word list.
    LookupView(Env& env, Navigator& navigator, const LookupContext& context, const ReaderLink& link);

private:
    Env& env_;
    LookupFeature feature_;
    ReaderLink link_;
    QVBoxLayout* column_;

    void render();
};
