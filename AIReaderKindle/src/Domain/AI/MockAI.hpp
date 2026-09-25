#pragma once

#include "Support/Json.hpp"
#include "ChatMessage.hpp"

#include <string>
#include <vector>

/// Stands in for the model when the endpoint is the mock endpoint.
///
/// It reads the same conversation a real model would and answers from the
/// dictionary material in it, so a mocked run still exercises the prompt, the
/// tool-calling loop and the JSON parsing.
namespace MockAI {

extern const std::vector<std::string> models;

ChatMessage reply(const std::vector<ChatMessage>& messages);

/// What a web search endpoint would answer, in the shape TinyFish uses,
/// so the `search_web` tool needs no network under the mock.
Json webSearch(const std::string& query);

}  // namespace MockAI
