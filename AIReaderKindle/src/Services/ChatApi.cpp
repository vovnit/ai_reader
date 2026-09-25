#include "ChatApi.hpp"

#include "../Domain/AI/MockAI.hpp"
#include "../Domain/AI/RequestQuirks.hpp"
#include "Http.hpp"

#include <algorithm>
#include <set>

namespace ChatApi {

namespace {

using Http::Response;
using Http::send;

Json requestBody(
    const AiSettings& settings,
    const std::vector<ChatMessage>& messages,
    const Json& tools,
    bool jsonMode,
    const std::set<RequestQuirk>& quirks)
{
    Json body = Json::object();
    body.set("model", settings.model);
    Json list = Json::array();
    for (const auto& message : messages) list.push(message.toJson());
    body.set("messages", list);
    body.set(quirks.count(RequestQuirk::CompletionTokens) ? "max_completion_tokens" : "max_tokens", 700);
    if (!quirks.count(RequestQuirk::DefaultTemperature)) body.set("temperature", 0.2);
    if (quirks.count(RequestQuirk::NoReasoning)) body.set("reasoning_effort", "none");
    if (tools.size() > 0) {
        body.set("tools", tools);
        body.set("tool_choice", "auto");
    }
    if (jsonMode) {
        Json format = Json::object();
        format.set("type", "json_object");
        body.set("response_format", format);
    }
    return body;
}

}  // namespace

void initialize() {
    Http::initialize();
}

ChatMessage chat(const AiSettings& settings, const std::vector<ChatMessage>& messages, const Json& tools, bool jsonMode) {
    if (settings.usesMock()) return MockAI::reply(messages);
    std::string url = settings.chatUrl();
    if (url.find("://") == std::string::npos) throw Error("“" + settings.endpoint + "” is not a valid endpoint URL.");

    std::string signature = settings.endpoint + "|" + settings.model;
    std::set<RequestQuirk> quirks = RequestQuirkStore::shared().quirks(signature);

    // A service that rejects a parameter says which one, so drop or rename it
    // and try again rather than failing the lookup.
    for (int attempt = 0; attempt <= requestQuirkCount; ++attempt) {
        std::string body = requestBody(settings, messages, tools, jsonMode, quirks).dump();
        Response response = send(url, settings.token(), &body, 45);

        if (response.status == 400) {
            auto quirk = requestQuirkNamed(response.body);
            if (quirk && quirks.insert(*quirk).second) {
                RequestQuirkStore::shared().learn(*quirk, signature);
                continue;
            }
        }
        if (response.status < 200 || response.status >= 300) {
            throw Error("The request failed (" + std::to_string(response.status) + "): " + response.body);
        }
        auto json = Json::parse(response.body);
        if (!json || json->at("choices").size() == 0) throw Error("The service returned no answer.");
        return ChatMessage::fromJson(json->at("choices").at(0).at("message"));
    }
    throw Error("The service returned no answer.");
}

std::vector<std::string> models(const AiSettings& settings) {
    if (settings.usesMock()) return MockAI::models;
    std::string url = settings.modelsUrl();
    if (url.find("://") == std::string::npos) throw Error("“" + settings.endpoint + "” is not a valid endpoint URL.");

    Response response = send(url, settings.token(), nullptr, 20);
    if (response.status < 200 || response.status >= 300) {
        throw Error("The request failed (" + std::to_string(response.status) + "): " + response.body);
    }
    auto json = Json::parse(response.body);
    if (!json) throw Error("The model list could not be read.");
    std::set<std::string> names;
    for (const auto& model : json->at("data").items()) {
        if (model.at("id").isString()) names.insert(model.at("id").string());
    }
    return std::vector<std::string>(names.begin(), names.end());
}

}  // namespace ChatApi
