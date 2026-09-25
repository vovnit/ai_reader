#pragma once

#include <optional>
#include <string>
#include <vector>

/// Where a reader is in a book, in terms that mean the same on every device:
/// the chapter, how far into it, and the words at that point. Offsets differ
/// between apps — bytes here, UTF-16 units on iOS — and between renderings,
/// so a place is found again by its words, and by its fraction when the
/// words cannot be found.
///
/// The words are compared loosely: every run of whitespace, breaking or not,
/// counts as one space, and illustration placeholders do not count at all,
/// since the two apps render those differently.
struct ReadingPlace {
    int chapter = 0;
    /// How far into the chapter's text, from 0 to 1.
    double fraction = 0;
    /// The text starting at the place, a few words of it, normalized.
    std::string snippet;

    /// How much text is kept as the snippet, in bytes, before it is cut back
    /// to a word boundary.
    static constexpr int snippetLength = 80;

    /// The place at byte `offset` of a chapter's text.
    static ReadingPlace at(int chapter, const std::string& chapterText, int offset);
    /// The byte offset in the chapter's text this place stands for: where the
    /// snippet is found, else the fraction.
    int resolve(const std::string& chapterText) const;

    bool operator==(const ReadingPlace& other) const {
        return chapter == other.chapter && fraction == other.fraction && snippet == other.snippet;
    }
    bool operator!=(const ReadingPlace& other) const { return !(*this == other); }
};
