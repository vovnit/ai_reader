#pragma once

#include "Support/Json.hpp"
#include "../Search/BookSearch.hpp"

#include <string>
#include <vector>

/// The tool that lets the model read the book: `search_book` finds a phrase
/// in the book being read, and in the other books of its group.
namespace SearchTool {

extern const char* const toolName;

/// How many passages a call returns to the model.
constexpr int passageLimit = 12;

Json tool();
std::string query(const std::string& arguments);

/// The passages as the model reads them: numbered, each with its book when
/// several are searched, and its chapter.
std::string summary(const std::string& query, const std::vector<SearchHit>& hits, bool severalBooks);

}  // namespace SearchTool
