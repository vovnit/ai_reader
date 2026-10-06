#pragma once

#include "Support/Json.hpp"
#include "../Books/Book.hpp"

#include <optional>
#include <string>

/// The tool that lets the model read past what it was given: `expand_context`
/// returns the text just before the passage a conversation is about — the
/// sentence of a lookup, the page of a chat — or just after it. Each call
/// reads one step further, within the chapter.
namespace ContextTool {

extern const char* const toolName;

enum class Direction { Before, After };

Json tool();
/// The direction asked for, or none when the model sent something else.
std::optional<Direction> direction(const std::string& arguments);

/// Reads one step past `window` in `chapter`, the text of its chapter,
/// widens the window by it, and says what was read as the model reads it.
std::string read(Direction direction, const std::string& chapter, BookPassage& window);

/// What the model is told when there is no passage to read around, or it
/// asked for neither direction.
extern const char* const nothingAround;
extern const char* const unknownDirection;

}  // namespace ContextTool
