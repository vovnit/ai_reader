#pragma once

#include "../../Domain/Reading/ReadingStyle.hpp"
#include "../../Services/Env.hpp"

#include <functional>

/// Type size, face, leading and margins. Every edit is saved and handed to
/// the reader, so the page behind re-renders as it is made.
class DisplayFeature {
public:
    DisplayFeature(Env& env, std::function<void(const ReadingStyle&)> apply);

    const ReadingStyle& style() const { return style_; }
    bool animatesTurns() const { return animatesTurns_; }

    void adjustScale(int direction);
    void adjustLineSpacing(int direction);
    void adjustMargin(int direction);
    void setFont(const std::string& name);
    /// Saved only; the reader reads it back when it returns to the front.
    void setAnimatesTurns(bool animates);

    std::function<void()> onChange;

private:
    Env& env_;
    std::function<void(const ReadingStyle&)> apply_;
    ReadingStyle style_;
    bool animatesTurns_;

    void changed();
};
