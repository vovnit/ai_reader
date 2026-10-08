#pragma once

#include <optional>
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

/// The quirk's name as it is written down — the iOS and web apps' names —
/// and the quirk a name stands for.
const char* requestQuirkName(RequestQuirk quirk);
std::optional<RequestQuirk> requestQuirkCalled(const std::string& name);
