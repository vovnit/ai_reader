#include "WebDav.hpp"

#include "../Support/XmlScanner.hpp"

#include <curl/curl.h>

#include <cctype>

namespace WebDav {

namespace {

struct Response {
    long status = 0;
    std::string body;
};

size_t collect(char* data, size_t size, size_t count, void* target) {
    static_cast<std::string*>(target)->append(data, size * count);
    return size * count;
}

Response send(const char* method, const std::string& url, const std::string* body, const SyncSettings& settings,
              const std::vector<std::string>& extraHeaders = {}) {
    CURL* curl = curl_easy_init();
    if (!curl) throw Error("The HTTP client could not be started.");

    Response response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    // Gives up on a stalled line, not a slow one: a book can take minutes.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    if (!settings.username.empty() || !settings.password.empty()) {
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(curl, CURLOPT_USERPWD, (settings.username + ":" + settings.password).c_str());
    }
    struct curl_slist* headers = nullptr;
    for (const auto& header : extraHeaders) headers = curl_slist_append(headers, header.c_str());
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    if (body) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body->size()));
    }

    CURLcode code = curl_easy_perform(curl);
    if (code == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) throw Error(std::string("The request failed: ") + curl_easy_strerror(code));
    return response;
}

void check(long status) {
    if (status == 401 || status == 403) throw Error("The server refused the user name or password.");
    if (status < 200 || status >= 300) throw Error("The server answered " + std::to_string(status) + ".");
}

std::string decode(const std::string& text) {
    std::string decoded;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size() && std::isxdigit(static_cast<unsigned char>(text[i + 1]))
            && std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
            decoded += static_cast<char>(std::stoi(text.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            decoded += text[i];
        }
    }
    return decoded;
}

std::string folder(const std::string& url) {
    auto slash = url.rfind('/');
    return slash == std::string::npos ? url : url.substr(0, slash);
}

/// Makes the folder, and the folders above it that are missing too: a
/// server makes only one level at a time, answering 409 when the one above
/// is not there.
void makeFolder(const std::string& url, const SyncSettings& settings, int depth = 0) {
    if (send("MKCOL", url, nullptr, settings).status != 409 || depth > 8) return;
    makeFolder(folder(url), settings, depth + 1);
    send("MKCOL", url, nullptr, settings);
}

}  // namespace

std::optional<std::string> download(const std::string& url, const SyncSettings& settings) {
    Response response = send("GET", url, nullptr, settings);
    if (response.status == 404) return std::nullopt;
    check(response.status);
    return response.body;
}

void upload(const std::string& url, const std::string& contents, const SyncSettings& settings,
            const std::string& type) {
    std::vector<std::string> headers = {"Content-Type: " + type};
    Response response = send("PUT", url, &contents, settings, headers);
    if (response.status == 409 || response.status == 404) {
        makeFolder(folder(url), settings);
        response = send("PUT", url, &contents, settings, headers);
    }
    check(response.status);
}

std::vector<std::string> list(const std::string& folderUrl, const SyncSettings& settings) {
    Response response = send("PROPFIND", folderUrl, nullptr, settings, {"Depth: 1"});
    if (response.status == 404) return {};
    check(response.status);
    return fileNames(response.body);
}

std::vector<std::string> fileNames(const std::string& multistatus) {
    std::vector<std::string> names;
    std::string href;
    bool isFolder = false;
    std::string element;
    auto close = [&] {
        while (!href.empty() && href.back() == '/') href.pop_back();
        auto slash = href.rfind('/');
        std::string name = decode(slash == std::string::npos ? href : href.substr(slash + 1));
        if (!isFolder && !name.empty()) names.push_back(name);
        href.clear();
        isFolder = false;
    };
    XmlScanner::scan(multistatus, {
        [&](const std::string& name, const XmlScanner::Attributes&) {
            element = name;
            if (name == "response" && !href.empty()) close();
            if (name == "collection") isFolder = true;
        },
        [&](const std::string&) { element.clear(); },
        [&](const std::string& text) {
            if (element == "href") href += text;
        },
    });
    if (!href.empty()) close();
    return names;
}

std::string escape(const std::string& name) {
    static const char* const hex = "0123456789ABCDEF";
    std::string escaped;
    for (char c : name) {
        auto byte = static_cast<unsigned char>(c);
        if (std::isalnum(byte) || c == '-' || c == '.' || c == '_' || c == '~') {
            escaped += c;
        } else {
            escaped += '%';
            escaped += hex[byte >> 4];
            escaped += hex[byte & 0xF];
        }
    }
    return escaped;
}

}  // namespace WebDav
