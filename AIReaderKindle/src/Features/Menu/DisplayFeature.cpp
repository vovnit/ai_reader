#include "DisplayFeature.hpp"

#include <algorithm>
#include <cmath>

DisplayFeature::DisplayFeature(Env& env, std::function<void(const ReadingStyle&)> apply)
    : env_(env), apply_(std::move(apply)), style_(env.settings.style()), animatesTurns_(env.settings.animatesTurns()) {}

static double stepped(double value, int direction, double step, double low, double high) {
    double next = value + direction * step;
    // Round to the step so repeated taps do not drift.
    next = std::round(next / step) * step;
    return std::min(std::max(next, low), high);
}

void DisplayFeature::adjustScale(int direction) {
    style_.scale = stepped(style_.scale, direction, ReadingStyle::scaleStep, ReadingStyle::scaleMin, ReadingStyle::scaleMax);
    changed();
}

void DisplayFeature::adjustLineSpacing(int direction) {
    style_.lineSpacing = stepped(style_.lineSpacing, direction, ReadingStyle::lineSpacingStep,
                                 ReadingStyle::lineSpacingMin, ReadingStyle::lineSpacingMax);
    changed();
}

void DisplayFeature::adjustMargin(int direction) {
    style_.margin = stepped(style_.margin, direction, ReadingStyle::marginStep, ReadingStyle::marginMin, ReadingStyle::marginMax);
    changed();
}

void DisplayFeature::setFont(const std::string& name) {
    style_.fontName = name == "Serif" ? "" : name;
    changed();
}

void DisplayFeature::setAnimatesTurns(bool animates) {
    animatesTurns_ = animates;
    env_.settings.saveAnimatesTurns(animates);
    if (onChange) onChange();
}

void DisplayFeature::changed() {
    env_.settings.saveStyle(style_);
    if (apply_) apply_(style_);
    if (onChange) onChange();
}
