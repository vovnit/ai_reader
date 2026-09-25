#pragma once

#include <string>
#include <vector>

/// How the book is rendered: the reader's own preference.
struct ReadingStyle {
    /// Multiplier on the base text size.
    double scale = 1.4;
    /// A font family to impose, or empty for the default serif.
    std::string fontName;
    /// Extra space between lines, in desktop pixels.
    double lineSpacing = 2;
    /// Space around the page, in desktop pixels.
    double margin = 24;

    /// Text size at scale 1 on a desktop; the screen scale multiplies it.
    static constexpr double basePixelSize = 16;
    static constexpr double scaleMin = 0.8, scaleMax = 2.6, scaleStep = 0.1;
    static constexpr double lineSpacingMin = 0, lineSpacingMax = 16, lineSpacingStep = 1;
    static constexpr double marginMin = 8, marginMax = 64, marginStep = 4;

    /// Families worth offering: the default, plus faces the Kindle ships.
    static const std::vector<std::string>& fontNames() {
        static const std::vector<std::string> names = {
            "Serif", "Sans", "Bookerly", "Caecilia", "Palatino", "Georgia", "Helvetica",
        };
        return names;
    }

    bool operator==(const ReadingStyle& other) const {
        return scale == other.scale && fontName == other.fontName
            && lineSpacing == other.lineSpacing && margin == other.margin;
    }
    bool operator!=(const ReadingStyle& other) const { return !(*this == other); }
};
