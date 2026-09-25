#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>

/// Parameters that some OpenAI-compatible services reject while others require
/// them. A service names the offending parameter in its error, so a request can
/// be adjusted and tried again.
enum class RequestQuirk {
    /// Newer OpenAI reasoning models take `max_completion_tokens` in place of `max_tokens`.
    CompletionTokens,
    /// Those models also only accept their default temperature.
    DefaultTemperature,
    /// Some of them refuse function tools unless reasoning is switched off.
    NoReasoning,
};

constexpr int requestQuirkCount = 3;

/// Reads the quirk a failed request's error body is describing, if any.
std::optional<RequestQuirk> requestQuirkNamed(const std::string& errorBody);

/// Remembers, for the rest of the session, which adjustments a given model
/// needed, so the cost of discovering them is paid once rather than on every
/// lookup.
class RequestQuirkStore {
public:
    static RequestQuirkStore& shared();

    std::set<RequestQuirk> quirks(const std::string& model);
    void learn(RequestQuirk quirk, const std::string& model);

private:
    std::mutex mutex_;
    std::map<std::string, std::set<RequestQuirk>> known_;
};
