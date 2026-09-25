#include "WebSearch.hpp"

#include "Http.hpp"

#include <glib.h>

#include <algorithm>
#include <cstdio>

namespace WebSearch {

namespace {

const char* const apiBase = "https://api.monid.ai/v1";
/// How long a search in the background is waited for, all polls together.
const int waitSeconds = 60;

Json parsed(const Http::Response& response) {
    if (response.status < 200 || response.status >= 300) {
        auto body = Json::parse(response.body);
        std::string message = body ? body->at("error").at("message").string() : "";
        if (message.empty() && body) message = body->at("message").string();
        throw Http::Error("The web search failed (" + std::to_string(response.status) + "): "
                          + (message.empty() ? response.body : message));
    }
    auto json = Json::parse(response.body);
    if (!json) throw Http::Error("The web search's answer could not be read.");
    return *json;
}

bool isDone(const Json& run) {
    const std::string& status = run.at("status").string();
    return status == "COMPLETED" || status == "FAILED" || status == "BLOCKED";
}

/// The provider's data out of a finished run, or why there is none.
Json output(const Json& run) {
    const std::string& status = run.at("status").string();
    if (status != "COMPLETED") {
        std::string reason = run.at("error").isString() ? run.at("error").string() : run.at("error").at("message").string();
        throw Http::Error("The web search did not complete (" + status + (reason.empty() ? "" : ": " + reason) + ").");
    }
    const Json& provider = run.at("providerResponse");
    double httpStatus = provider.at("httpStatus").number(200);
    if (httpStatus >= 400) {
        std::string reason = provider.at("error").at("message").string();
        throw Http::Error("The search provider answered " + std::to_string(static_cast<int>(httpStatus))
                          + (reason.empty() ? "." : ": " + reason));
    }
    // Every run costs something; the log says how much.
    const Json& cost = run.at("cost");
    if (cost.isObject()) {
        std::fprintf(stderr, "web search %s%s cost %.4f %s\n", run.at("provider").string().c_str(),
                     run.at("endpoint").string().c_str(), cost.at("value").number(), cost.at("currency").string().c_str());
    }
    return provider.at("data").isNull() ? run.at("output") : provider.at("data");
}

}  // namespace

Json search(const WebSearchSettings& settings, const std::string& query, const std::string& language) {
    auto input = Json::parse(settings.request(query, language));
    if (!input || !input->isObject()) throw Http::Error("The web search input is not valid JSON; check it in Settings.");

    // The input is sent as it is, `queryParams`, `body` and `pathParams`
    // inside it, the way Monid's own CLI sends what it is given.
    Json request = Json::object();
    request.set("provider", settings.provider);
    request.set("endpoint", settings.endpoint);
    request.set("input", *input);

    std::string body = request.dump();
    Json run = parsed(Http::send(std::string(apiBase) + "/run", settings.apiKey, &body, 45));

    // A provider that works in the background is polled, a little less
    // often each time, the way Monid's own client does.
    int waited = 0;
    double delay = 1;
    while (!isDone(run) && waited < waitSeconds) {
        g_usleep(static_cast<gulong>(delay * G_USEC_PER_SEC));
        waited += static_cast<int>(delay);
        delay = std::min(delay * 1.5, 10.0);
        std::string runId = run.at("runId").string();
        if (runId.empty()) throw Http::Error("The web search returned no run to wait for.");
        run = parsed(Http::send(std::string(apiBase) + "/runs/" + runId, settings.apiKey, nullptr, 30));
    }
    if (!isDone(run)) throw Http::Error("The web search took too long.");
    return output(run);
}

}  // namespace WebSearch
