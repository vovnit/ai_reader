#include "RequestQuirks.hpp"

#include "Support/Json.hpp"
#include "../../Support/Text.hpp"

std::optional<RequestQuirk> requestQuirkNamed(const std::string& errorBody) {
    auto json = Json::parse(errorBody);
    if (!json) return std::nullopt;
    const Json& error = json->at("error");
    std::string parameter = error.at("param").string();
    std::string message = error.at("message").string();

    if (parameter == "max_tokens" || Text::contains(message, "max_completion_tokens")) {
        return RequestQuirk::CompletionTokens;
    }
    if (parameter == "temperature") return RequestQuirk::DefaultTemperature;
    if (parameter == "reasoning_effort") return RequestQuirk::NoReasoning;
    return std::nullopt;
}

const char* requestQuirkName(RequestQuirk quirk) {
    switch (quirk) {
        case RequestQuirk::CompletionTokens: return "completionTokens";
        case RequestQuirk::DefaultTemperature: return "defaultTemperature";
        case RequestQuirk::NoReasoning: return "noReasoning";
    }
    return "";
}

std::optional<RequestQuirk> requestQuirkCalled(const std::string& name) {
    for (auto quirk : {RequestQuirk::CompletionTokens, RequestQuirk::DefaultTemperature, RequestQuirk::NoReasoning}) {
        if (name == requestQuirkName(quirk)) return quirk;
    }
    return std::nullopt;
}
