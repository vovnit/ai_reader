#include "Settings.hpp"

#include "../Domain/Books/RemoteBookName.hpp"

#include "Support/Json.hpp"
#include "../Support/Text.hpp"

#include <glib.h>

#include <cstdio>

const char* const AiSettings::mockEndpoint = "mock://ai";

/// No token ships with the app. Until one is entered, `MISTRAL_API_KEY` in
/// the environment stands in, for development — as in the iOS app.
AiSettings AiSettings::defaults() {
    const char* key = g_getenv("MISTRAL_API_KEY");
    return {"https://api.mistral.ai/v1", key ? key : "", "", "mistral-medium-3.5"};
}

bool AiSettings::isOpenAI() const {
    return Text::contains(Text::lower(endpoint), "api.openai.com");
}

std::string AiSettings::token() const {
    return isOpenAI() && !openAIKey.empty() ? openAIKey : apiKey;
}

bool AiSettings::usesMock() const {
    return Text::trim(endpoint) == mockEndpoint;
}

std::string AiSettings::chatUrl() const { return url("chat/completions"); }
std::string AiSettings::modelsUrl() const { return url("models"); }

std::string AiSettings::url(const std::string& path) const {
    std::string base = Text::trim(endpoint);
    while (!base.empty() && base.back() == '/') base.pop_back();
    return base + "/" + path;
}

bool SyncSettings::isConfigured() const {
    return !Text::trim(url).empty();
}

std::string SyncSettings::fileUrl() const {
    std::string base = Text::trim(url);
    while (!base.empty() && base.back() == '/') base.pop_back();
    return base + "/aireader-sync.json";
}

std::string SyncSettings::booksUrl() const {
    std::string base = Text::trim(url);
    while (!base.empty() && base.back() == '/') base.pop_back();
    return base + "/" + RemoteBookName::folder + "/";
}

/// TinyFish's search, which Monid lists at no charge: a GET whose query
/// parameters are the words and the language, answered with titles,
/// addresses and snippets.
WebSearchSettings WebSearchSettings::defaults() {
    return {"", "tinyfish", "/search", R"({"queryParams": {"query": "$query", "language": "$language"}})"};
}

bool WebSearchSettings::isConfigured() const {
    return !Text::trim(apiKey).empty() && !Text::trim(provider).empty() && !Text::trim(endpoint).empty();
}

std::string WebSearchSettings::request(const std::string& query, const std::string& language) const {
    // Dumped as JSON strings and unquoted, so quotes and newlines in the
    // query cannot break out of the template.
    auto escaped = [](const std::string& value) {
        std::string quoted = Json(value).dump();
        return quoted.substr(1, quoted.size() - 2);
    };
    std::string filled = Text::replaceAll(input, "$query", escaped(query));
    return Text::replaceAll(filled, "$language", escaped(language.empty() ? "en" : language));
}

// MARK: - Store

namespace {

struct KeyFile {
    GKeyFile* file = g_key_file_new();
    explicit KeyFile(const std::string& path) {
        g_key_file_load_from_file(file, path.c_str(), G_KEY_FILE_NONE, nullptr);
    }
    ~KeyFile() { g_key_file_free(file); }

    std::string string(const char* group, const char* key, const std::string& fallback) const {
        gchar* value = g_key_file_get_string(file, group, key, nullptr);
        std::string result = value ? value : fallback;
        g_free(value);
        return result;
    }

    double number(const char* group, const char* key, double fallback) const {
        GError* error = nullptr;
        double value = g_key_file_get_double(file, group, key, &error);
        if (error) {
            g_error_free(error);
            return fallback;
        }
        return value;
    }

    bool save(const std::string& path) {
        gchar* data = g_key_file_to_data(file, nullptr, nullptr);
        bool ok = data && g_file_set_contents(path.c_str(), data, -1, nullptr);
        g_free(data);
        return ok;
    }
};

}  // namespace

SettingsStore::SettingsStore(std::string path) : path_(std::move(path)) {}

AiSettings SettingsStore::ai() const {
    KeyFile file(path_);
    AiSettings defaults = AiSettings::defaults();
    AiSettings settings{
        file.string("ai", "endpoint", defaults.endpoint),
        file.string("ai", "token", defaults.apiKey),
        file.string("ai", "openai_token", defaults.openAIKey),
        file.string("ai", "model", defaults.model),
    };
    if (!runEndpoint_.empty()) settings.endpoint = runEndpoint_;
    if (!runModel_.empty()) settings.model = runModel_;
    return settings;
}

void SettingsStore::saveAi(const AiSettings& settings) {
    KeyFile file(path_);
    // A value overridden for this run is not the reader's setting; leave the
    // file's own value alone.
    if (runEndpoint_.empty()) g_key_file_set_string(file.file, "ai", "endpoint", settings.endpoint.c_str());
    g_key_file_set_string(file.file, "ai", "token", settings.apiKey.c_str());
    g_key_file_set_string(file.file, "ai", "openai_token", settings.openAIKey.c_str());
    if (runModel_.empty()) g_key_file_set_string(file.file, "ai", "model", settings.model.c_str());
    file.save(path_);
}

WebSearchSettings SettingsStore::webSearch() const {
    KeyFile file(path_);
    WebSearchSettings defaults = WebSearchSettings::defaults();
    return {
        file.string("web", "monid_token", defaults.apiKey),
        file.string("web", "provider", defaults.provider),
        file.string("web", "endpoint", defaults.endpoint),
        file.string("web", "input", defaults.input),
    };
}

void SettingsStore::saveWebSearch(const WebSearchSettings& settings) {
    KeyFile file(path_);
    g_key_file_set_string(file.file, "web", "monid_token", settings.apiKey.c_str());
    g_key_file_set_string(file.file, "web", "provider", settings.provider.c_str());
    g_key_file_set_string(file.file, "web", "endpoint", settings.endpoint.c_str());
    g_key_file_set_string(file.file, "web", "input", settings.input.c_str());
    file.save(path_);
}

SyncSettings SettingsStore::sync() const {
    KeyFile file(path_);
    return {
        file.string("sync", "url", ""),
        file.string("sync", "user", ""),
        file.string("sync", "password", ""),
    };
}

void SettingsStore::saveSync(const SyncSettings& settings) {
    KeyFile file(path_);
    g_key_file_set_string(file.file, "sync", "url", settings.url.c_str());
    g_key_file_set_string(file.file, "sync", "user", settings.username.c_str());
    g_key_file_set_string(file.file, "sync", "password", settings.password.c_str());
    file.save(path_);
}

ReadingStyle SettingsStore::style() const {
    KeyFile file(path_);
    ReadingStyle defaults;
    ReadingStyle style;
    style.scale = file.number("display", "scale", defaults.scale);
    style.fontName = file.string("display", "font", defaults.fontName);
    style.lineSpacing = file.number("display", "line_spacing", defaults.lineSpacing);
    style.margin = file.number("display", "margin", defaults.margin);
    return style;
}

void SettingsStore::saveStyle(const ReadingStyle& style) {
    KeyFile file(path_);
    // Written as short decimals so the file stays readable by hand.
    auto number = [&](const char* key, double value) {
        char buffer[32];
        std::snprintf(buffer, sizeof buffer, "%.2f", value);
        g_key_file_set_string(file.file, "display", key, buffer);
    };
    number("scale", style.scale);
    g_key_file_set_string(file.file, "display", "font", style.fontName.c_str());
    number("line_spacing", style.lineSpacing);
    number("margin", style.margin);
    file.save(path_);
}

bool SettingsStore::animatesTurns() const {
    KeyFile file(path_);
    return file.string("display", "turn_animation", "on") != "off";
}

void SettingsStore::saveAnimatesTurns(bool animates) {
    KeyFile file(path_);
    g_key_file_set_string(file.file, "display", "turn_animation", animates ? "on" : "off");
    file.save(path_);
}

void SettingsStore::overrideForRun(const std::string& endpoint, const std::string& model) {
    runEndpoint_ = endpoint;
    runModel_ = model;
}
