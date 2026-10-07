#pragma once

#include "../Domain/AI/ChatMessage.hpp"
#include "Support/Json.hpp"
#include "Http.hpp"
#include "Settings.hpp"

#include <string>
#include <vector>

/// Chat completions and the model list, against any OpenAI-compatible service.
/// Failures are thrown as `ChatApi::Error` with a message fit to show.
namespace ChatApi {

using Error = Http::Error;

/// `maxTokens` bounds the answer: enough for a lookup unless said otherwise.
ChatMessage chat(
    const AiSettings& settings,
    const std::vector<ChatMessage>& messages,
    const Json& tools = Json::array(),
    bool jsonMode = false,
    int maxTokens = 700);

/// The model names the endpoint offers, sorted.
std::vector<std::string> models(const AiSettings& settings);

/// Once per process, before any request.
void initialize();

}  // namespace ChatApi
