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

RequestQuirkStore& RequestQuirkStore::shared() {
    static RequestQuirkStore store;
    return store;
}

std::set<RequestQuirk> RequestQuirkStore::quirks(const std::string& model) {
    std::lock_guard<std::mutex> lock(mutex_);
    return known_[model];
}

void RequestQuirkStore::learn(RequestQuirk quirk, const std::string& model) {
    std::lock_guard<std::mutex> lock(mutex_);
    known_[model].insert(quirk);
}
