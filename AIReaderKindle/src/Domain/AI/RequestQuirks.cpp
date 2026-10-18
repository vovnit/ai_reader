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
    // Anthropic names no parameter; its message says what it refused.
    if (Text::contains(message, "effort") || Text::contains(message, "output_config")) return RequestQuirk::NoEffort;
    return std::nullopt;
}

const char* requestQuirkName(RequestQuirk quirk) {
    switch (quirk) {
        case RequestQuirk::CompletionTokens: return "completionTokens";
        case RequestQuirk::DefaultTemperature: return "defaultTemperature";
        case RequestQuirk::NoReasoning: return "noReasoning";
        case RequestQuirk::NoEffort: return "noEffort";
    }
    return "";
}

std::optional<RequestQuirk> requestQuirkCalled(const std::string& name) {
    for (auto quirk : {RequestQuirk::CompletionTokens, RequestQuirk::DefaultTemperature, RequestQuirk::NoReasoning, RequestQuirk::NoEffort}) {
        if (name == requestQuirkName(quirk)) return quirk;
    }
    return std::nullopt;
}
