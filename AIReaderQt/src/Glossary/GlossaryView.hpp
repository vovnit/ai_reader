#pragma once

#include "Common/Screen.hpp"
#include "Features/Glossary/GlossaryFeature.hpp"

class QVBoxLayout;
class ReaderFeature;

/// A book's offline glossary: how many of its words have a definition, what
/// defining the rest should cost, and the button that does it.
class GlossaryView : public Screen {
public:
    GlossaryView(Env& env, Navigator& navigator, ReaderFeature& reader);

private:
    GlossaryFeature feature_;
    QVBoxLayout* column_;

    void render();
};
