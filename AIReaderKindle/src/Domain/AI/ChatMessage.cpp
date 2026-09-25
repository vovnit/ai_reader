#include "ChatMessage.hpp"

Json ChatMessage::toJson() const {
    Json json = Json::object();
    json.set("role", role);
    json.set("content", content ? Json(*content) : Json(nullptr));
    if (!toolCalls.empty()) {
        Json calls = Json::array();
        for (const auto& call : toolCalls) {
            Json function = Json::object();
            function.set("name", call.name);
            function.set("arguments", call.arguments);
            Json entry = Json::object();
            entry.set("id", call.id);
            // Services require this back on every call they made, even the
            // ones that leave it out of their own reply.
            entry.set("type", "function");
            entry.set("function", function);
            calls.push(entry);
        }
        json.set("tool_calls", calls);
    }
    if (!toolCallId.empty()) json.set("tool_call_id", toolCallId);
    return json;
}

ChatMessage ChatMessage::fromJson(const Json& json) {
    ChatMessage message;
    message.role = json.at("role").string();
    if (json.at("content").isString()) message.content = json.at("content").string();
    for (const auto& call : json.at("tool_calls").items()) {
        const Json& function = call.at("function");
        const Json& arguments = function.at("arguments");
        message.toolCalls.push_back(ToolCall{
            call.at("id").string(),
            function.at("name").string(),
            // Some services return the arguments as an object rather than a string.
            arguments.isString() ? arguments.string() : arguments.dump(),
        });
    }
    message.toolCallId = json.at("tool_call_id").string();
    return message;
}
