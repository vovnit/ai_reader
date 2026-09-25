#pragma once

#include "ChatMessage.hpp"
#include "../Search/BookSearch.hpp"

#include <string>
#include <vector>

/// The conversation that asks what a name or a term means *in this book*:
/// the passages where it has appeared so far are the only source.
namespace XRayPrompt {

extern const char* const system;

/// The passages gathered before asking; at most this many go in.
constexpr int passageLimit = 12;

std::vector<ChatMessage> messages(const std::string& term, const std::vector<SearchHit>& hits, bool severalBooks);

}  // namespace XRayPrompt
