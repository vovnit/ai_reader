#include "Gutenberg.hpp"

#include "../../Support/Text.hpp"
#include "TextChapters.hpp"

#include <algorithm>

namespace Gutenberg {

namespace {

/// How far into the file the header's "Title:" and "Author:" lines are
/// looked for.
constexpr size_t headerLines = 60;

bool isMarker(const std::string& lower, const std::string& words) {
    return Text::startsWith(lower, "***") && Text::startsWith(TextChapters::collapsed(lower.substr(3)), words);
}

bool isStart(const std::string& lower) {
    return isMarker(lower, "start of");
}

bool isEnd(const std::string& lower) {
    return isMarker(lower, "end of") || Text::startsWith(lower, "end of the project gutenberg") || Text::startsWith(lower, "end of project gutenberg");
}

}  // namespace

std::optional<Parts> parts(const std::vector<std::string>& lines) {
    std::optional<size_t> start;
    std::optional<size_t> end;
    for (size_t i = 0; i < lines.size() && !end; ++i) {
        std::string lower = Text::lower(TextChapters::collapsed(lines[i]));
        if (!start && isStart(lower)) start = i;
        else if (start && isEnd(lower)) end = i;
    }
    if (!end) return std::nullopt;

    auto field = [&](const std::string& name) -> std::string {
        for (size_t i = 0; i < std::min(*start, headerLines); ++i) {
            std::string text = TextChapters::collapsed(lines[i]);
            if (Text::startsWith(text, name)) return TextChapters::collapsed(text.substr(name.size()));
        }
        return "";
    };
    return Parts{*start, *end, field("Title:"), field("Author:")};
}

}  // namespace Gutenberg
