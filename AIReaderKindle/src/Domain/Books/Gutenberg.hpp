#pragma once

#include <optional>
#include <string>
#include <vector>

/// A Project Gutenberg file wraps its book in a header, which names it, and
/// the license: the book lies between the line "*** START OF …" and the
/// line "*** END OF …", or "End of the Project Gutenberg EBook of …", which
/// older files put before it. The plain-text reader hands its lines here,
/// the HTML reader its paragraphs, so a book reads the same from either
/// file. The same as the web app's `Gutenberg.js` and the iOS app's
/// `Gutenberg.swift`.
namespace Gutenberg {

struct Parts {
    /// Where the two markers are.
    size_t start = 0;
    size_t end = 0;
    /// What the header names the book; empty when it does not.
    std::string title;
    std::string author;
};

/// Nothing when `lines` are not a Gutenberg file's.
std::optional<Parts> parts(const std::vector<std::string>& lines);

}  // namespace Gutenberg
