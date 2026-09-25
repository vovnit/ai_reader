#include "Http.hpp"

#include <curl/curl.h>

#include <cstdlib>

namespace Http {

namespace {

size_t collect(char* data, size_t size, size_t count, void* target) {
    static_cast<std::string*>(target)->append(data, size * count);
    return size * count;
}

}  // namespace

Response send(const std::string& url, const std::string& apiKey, const std::string* body, long timeout) {
    CURL* curl = curl_easy_init();
    if (!curl) throw Error("The HTTP client could not be started.");

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    if (!apiKey.empty()) headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());

    Response response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    // A libcurl built on one distribution looks for certificates where that
    // one keeps them; a copy carried elsewhere, as in an AppImage, is told.
    if (const char* certificates = std::getenv("CURL_CA_BUNDLE")) curl_easy_setopt(curl, CURLOPT_CAINFO, certificates);
    if (body) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body->size()));
    }

    CURLcode code = curl_easy_perform(curl);
    if (code == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) throw Error(std::string("The request failed: ") + curl_easy_strerror(code));
    if (response.status == 429) throw Error("The service is rate limiting this token. Try again shortly.");
    return response;
}

void initialize() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

}  // namespace Http
