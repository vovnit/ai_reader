#pragma once

#include "Support/Json.hpp"
#include "Settings.hpp"

#include <string>

/// A web search through Monid: `POST /v1/run` names the provider's endpoint
/// and its input; a provider that answers at once returns the output, and
/// one that runs in the background returns a run to poll at `/v1/runs/{id}`
/// until it is done. Throws `Http::Error` with a message fit to show.
namespace WebSearch {

/// The provider's answer: whatever JSON the endpoint returned. `language`
/// is the book's, for endpoints that take one.
Json search(const WebSearchSettings& settings, const std::string& query, const std::string& language);

}  // namespace WebSearch
