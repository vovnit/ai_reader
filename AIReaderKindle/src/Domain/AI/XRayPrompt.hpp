#pragma once

#include "ChatMessage.hpp"
#include "../Search/BookSearch.hpp"

#include <string>
#include <vector>

/// The conversation that asks what a name or a term means *in this book*:
/// the passages where it has appeared so far are the only source.
namespace XRayPrompt {

/// `language` is the one the answer is written in.
std::string system(const std::string& language);

/// The passages gathered before asking; at most this many go in.
constexpr int passageLimit = 12;

std::vector<ChatMessage> messages(const std::string& term, const std::vector<SearchHit>& hits, bool severalBooks, const std::string& language);

}  // namespace XRayPrompt
