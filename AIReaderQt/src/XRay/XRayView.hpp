#pragma once

#include "Common/Screen.hpp"
#include "Features/Reader/ReaderLink.hpp"
#include "Features/XRay/XRayFeature.hpp"

class QVBoxLayout;

/// What the book itself has said about a name or a word so far, and the
/// passages it was drawn from.
class XRayView : public Screen {
public:
    XRayView(Env& env, Navigator& navigator, const std::string& term, const ReaderLink& link);

    /// Asks for the term, then opens its X-ray.
    static void ask(Env& env, Navigator& navigator, const ReaderLink& link, QWidget* parent);

private:
    Env& env_;
    XRayFeature feature_;
    ReaderLink link_;
    QVBoxLayout* column_;

    void render();
};
