#pragma once

#include "Common/Screen.hpp"
#include "Features/Menu/DisplayFeature.hpp"

class QComboBox;
class QLabel;
class ReaderFeature;

/// Type size, face, leading and margins, applied to the page behind as they
/// are changed.
class DisplayView : public Screen {
public:
    DisplayView(Env& env, Navigator& navigator, ReaderFeature& reader);

private:
    DisplayFeature feature_;
    QLabel* scale_;
    QLabel* spacing_;
    QLabel* margin_;
    QComboBox* face_;
    QLabel* preview_;

    void render();
};
