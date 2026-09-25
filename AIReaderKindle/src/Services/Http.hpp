#pragma once

#include <stdexcept>
#include <string>

/// One JSON request over libcurl, shared by the services that talk to an
/// API with a bearer token. Failures are thrown as `Http::Error` with a
/// message fit to show.
namespace Http {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Response {
    long status = 0;
    std::string body;
};

/// A POST with `body`, or a GET without one. A 4xx/5xx status is returned,
/// not thrown, so the caller can read the error body.
Response send(const std::string& url, const std::string& apiKey, const std::string* body, long timeout);

/// Once per process, before any request.
void initialize();

}  // namespace Http
