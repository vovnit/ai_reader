#include "ContextTool.hpp"

#include "../../Support/Text.hpp"
#include "../Search/BookSearch.hpp"
#include "ToolSchema.hpp"

#include <algorithm>

namespace ContextTool {

const char* const toolName = "expand_context";
const char* const nothingAround = "There is no book text around this to read.";
const char* const unknownDirection = "The direction must be “before” or “after”.";

Json tool() {
    return ToolSchema::function(
        toolName,
        "Возвращает текст книги, который идёт прямо перед тем местом, о котором речь (предложением "
        "или страницей), или сразу после него. Каждый следующий вызов в ту же сторону читает дальше. "
        "Используй, когда для понимания не хватает соседнего текста: к кому относится местоимение, "
        "кто говорит, о чём шла речь абзацем выше.",
        "direction",
        "\"before\" — текст перед этим местом, \"after\" — текст после него.",
        {"before", "after"});
}

std::optional<Direction> direction(const std::string& arguments) {
    std::string value = ToolSchema::argument(arguments, "direction", "");
    if (value == "before") return Direction::Before;
    if (value == "after") return Direction::After;
    return std::nullopt;
}

std::string read(Direction direction, const std::string& chapter, BookPassage& window) {
    int size = static_cast<int>(chapter.size());
    window.start = std::clamp(window.start, 0, size);
    window.end = std::clamp(window.end, window.start, size);
    if (direction == Direction::Before) {
        if (window.start == 0) return "Nothing comes before it: the chapter begins there.";
        int from = BookSearch::stepBack(chapter, window.start);
        std::string text = Text::trim(chapter.substr(from, window.start - from));
        window.start = from;
        return "Before it in the book:\n" + text + (from == 0 ? "\n(The chapter begins here.)" : "");
    }
    if (window.end == size) return "Nothing comes after it: the chapter ends there.";
    int to = BookSearch::stepForward(chapter, window.end);
    std::string text = Text::trim(chapter.substr(window.end, to - window.end));
    window.end = to;
    return "After it in the book:\n" + text + (to == size ? "\n(The chapter ends here.)" : "");
}

}  // namespace ContextTool
