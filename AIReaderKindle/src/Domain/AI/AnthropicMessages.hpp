#pragma once

#include "ChatMessage.hpp"
#include "RequestQuirks.hpp"
#include "Support/Json.hpp"

#include <optional>
#include <set>
#include <string>
#include <vector>

/// Anthropic's Messages API, which its endpoint speaks in place of chat
/// completions: the request made from the same messages and tools, and the
/// reply read back into a `ChatMessage`. A reply keeps its content blocks as
/// they came, thinking included, and they go back unchanged in the requests
/// that follow: the API checks that they do.
namespace AnthropicMessages {

/// The API version every request names.
extern const char* const version;

/// `toolsAllowed` is false for the request that must be answered in words.
Json body(
    const std::string& model,
    const std::vector<ChatMessage>& messages,
    const Json& tools,
    bool toolsAllowed,
    int maxTokens,
    const std::set<RequestQuirk>& quirks);

/// The reply as a message, or nothing when it holds none. A refusal throws
/// `std::runtime_error` with a message fit to show.
std::optional<ChatMessage> reply(const Json& json);

}  // namespace AnthropicMessages
