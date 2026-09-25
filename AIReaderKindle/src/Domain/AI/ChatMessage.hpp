#pragma once

#include "Support/Json.hpp"

#include <optional>
#include <string>
#include <vector>

/// One message in a chat completion exchange.
struct ChatMessage {
    struct ToolCall {
        std::string id;
        std::string name;
        /// A JSON object, encoded as a string, as the API returns it.
        std::string arguments;
    };

    std::string role;
    std::optional<std::string> content;
    std::vector<ToolCall> toolCalls;
    std::string toolCallId;

    static ChatMessage system(const std::string& content) { return {"system", content, {}, ""}; }
    static ChatMessage user(const std::string& content) { return {"user", content, {}, ""}; }
    static ChatMessage assistant(const std::string& content) { return {"assistant", content, {}, ""}; }
    static ChatMessage toolResult(const std::string& content, const std::string& callId) {
        return {"tool", content, {}, callId};
    }

    Json toJson() const;
    static ChatMessage fromJson(const Json& json);
};
