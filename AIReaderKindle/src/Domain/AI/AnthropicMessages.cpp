#include "AnthropicMessages.hpp"

#include <stdexcept>

namespace AnthropicMessages {

const char* const version = "2023-06-01";

namespace {

Json input(const std::string& arguments) {
    auto parsed = Json::parse(arguments);
    return parsed && parsed->isObject() ? *parsed : Json::object();
}

Json blocks(const ChatMessage& message) {
    if (message.blocks.isArray()) return message.blocks;
    Json blocks = Json::array();
    if (!message.toolCallId.empty()) {
        Json result = Json::object();
        result.set("type", "tool_result");
        result.set("tool_use_id", message.toolCallId);
        result.set("content", message.content.value_or(""));
        return blocks.push(result);
    }
    if (message.content && !message.content->empty()) {
        Json text = Json::object();
        text.set("type", "text");
        text.set("text", *message.content);
        blocks.push(text);
    }
    for (const auto& call : message.toolCalls) {
        Json use = Json::object();
        use.set("type", "tool_use");
        use.set("id", call.id);
        use.set("name", call.name);
        use.set("input", input(call.arguments));
        blocks.push(use);
    }
    return blocks;
}

/// Tool results travel in a user turn and the roles must alternate, so
/// neighbours of one role are joined.
Json turns(const std::vector<ChatMessage>& messages) {
    std::vector<std::pair<std::string, Json>> joined;
    for (const auto& message : messages) {
        if (message.role == "system") continue;
        std::string role = message.role == "assistant" ? "assistant" : "user";
        Json content = blocks(message);
        if (content.size() == 0) continue;
        if (!joined.empty() && joined.back().first == role) {
            for (const auto& block : content.items()) joined.back().second.push(block);
        } else {
            joined.emplace_back(role, content);
        }
    }
    Json turns = Json::array();
    for (const auto& [role, content] : joined) {
        Json turn = Json::object();
        turn.set("role", role);
        turn.set("content", content);
        turns.push(turn);
    }
    return turns;
}

}  // namespace

Json body(const std::string& model, const std::vector<ChatMessage>& messages, const Json& tools, bool toolsAllowed,
          int maxTokens, const std::set<RequestQuirk>& quirks) {
    Json body = Json::object();
    body.set("model", model);
    body.set("max_tokens", maxTokens);
    std::string system;
    for (const auto& message : messages) {
        if (message.role != "system" || !message.content || message.content->empty()) continue;
        system += (system.empty() ? "" : "\n\n") + *message.content;
    }
    if (!system.empty()) body.set("system", system);
    body.set("messages", turns(messages));
    // Newer models think before answering, and the thinking counts against
    // `max_tokens`: a lookup wants little of it.
    if (!quirks.count(RequestQuirk::NoEffort)) {
        Json config = Json::object();
        config.set("effort", "low");
        body.set("output_config", config);
    }
    if (tools.size() > 0) {
        Json listed = Json::array();
        for (const auto& tool : tools.items()) {
            const Json& function = tool.at("function");
            Json entry = Json::object();
            entry.set("name", function.at("name"));
            entry.set("description", function.at("description"));
            entry.set("input_schema", function.at("parameters"));
            listed.push(entry);
        }
        body.set("tools", listed);
        // A conversation that has called the tools must still list them, so
        // they are kept and only their use is forbidden.
        if (!toolsAllowed) {
            Json choice = Json::object();
            choice.set("type", "none");
            body.set("tool_choice", choice);
        }
    }
    return body;
}

std::optional<ChatMessage> reply(const Json& json) {
    if (json.at("stop_reason").string() == "refusal") throw std::runtime_error("The model declined to answer.");
    const Json& content = json.at("content");
    if (!content.isArray()) return std::nullopt;
    ChatMessage message = ChatMessage::assistant("");
    std::string text;
    for (const auto& block : content.items()) {
        const std::string& type = block.at("type").string();
        if (type == "text") text += block.at("text").string();
        if (type == "tool_use") {
            const Json& input = block.at("input");
            message.toolCalls.push_back({block.at("id").string(), block.at("name").string(), input.isObject() ? input.dump() : "{}"});
        }
    }
    message.content = text.empty() ? std::nullopt : std::optional<std::string>(text);
    message.blocks = content;
    return message;
}

}  // namespace AnthropicMessages
