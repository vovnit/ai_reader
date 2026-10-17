#pragma once

#include "../Domain/AI/ChatMessage.hpp"
#include "Support/Json.hpp"
#include "Http.hpp"
#include "Settings.hpp"

#include <string>
#include <vector>

/// Chat completions and the model list, against any OpenAI-compatible service,
/// or Anthropic's in its own Messages API. Failures are thrown as
/// `ChatApi::Error` with a message fit to show.
namespace ChatApi {

using Error = Http::Error;

/// Enough for a lookup.
constexpr int lookupTokens = 700;

/// `maxTokens` bounds the answer. `toolsAllowed` false keeps the tools listed
/// but lets the model only answer, for the last request of a conversation
/// that has used them.
ChatMessage chat(
    const AiSettings& settings,
    const std::vector<ChatMessage>& messages,
    const Json& tools = Json::array(),
    bool jsonMode = false,
    int maxTokens = lookupTokens,
    bool toolsAllowed = true);

/// The model names the endpoint offers, sorted.
std::vector<std::string> models(const AiSettings& settings);

/// Once per process, before any request.
void initialize();

}  // namespace ChatApi
