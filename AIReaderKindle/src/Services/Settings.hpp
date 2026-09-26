#pragma once

#include "../Domain/Reading/ReadingStyle.hpp"

#include <string>

/// Which model answers word lookups, and where to reach it. Any
/// OpenAI-compatible endpoint works: the base URL is extended with
/// `/chat/completions` and `/models`.
struct AiSettings {
    std::string endpoint;
    /// The token for the endpoint above.
    std::string apiKey;
    /// A token kept for OpenAI, used instead whenever the endpoint is theirs,
    /// so switching between services does not mean retyping tokens.
    std::string openAIKey;
    std::string model;
    /// The language explanations and answers are written in, as the model
    /// is told it: a name such as "Russian" or "English".
    std::string language = "Russian";

    /// The token to send to the current endpoint.
    std::string token() const;
    bool isOpenAI() const;

    /// Not a real address. Pointing the app here answers lookups from
    /// `MockAI` instead of the network, which is how the app is tested
    /// without spending requests.
    static const char* const mockEndpoint;

    static AiSettings defaults();
    bool usesMock() const;
    std::string chatUrl() const;
    std::string modelsUrl() const;

    bool operator==(const AiSettings& other) const {
        return endpoint == other.endpoint && apiKey == other.apiKey
            && openAIKey == other.openAIKey && model == other.model && language == other.language;
    }
    bool operator!=(const AiSettings& other) const { return !(*this == other); }

private:
    std::string url(const std::string& path) const;
};

/// Where the sync file lives: a WebDAV folder and the account that may
/// write to it. Nothing is synced until a URL is given.
struct SyncSettings {
    std::string url;
    std::string username;
    std::string password;

    bool isConfigured() const;
    /// The sync file inside the folder.
    std::string fileUrl() const;
    /// The books' folder inside it, with a trailing slash.
    std::string booksUrl() const;

    bool operator==(const SyncSettings& other) const {
        return url == other.url && username == other.username && password == other.password;
    }
    bool operator!=(const SyncSettings& other) const { return !(*this == other); }
};

/// Web search for the model, through Monid (monid.ai): one key reaches the
/// search endpoints of many providers, some free and most paid per call.
/// Nothing is offered to the model until a key is given.
struct WebSearchSettings {
    std::string apiKey;
    /// Which endpoint answers, as `monid discover -q "web search"` lists
    /// them: a provider slug and an endpoint path.
    std::string provider;
    std::string endpoint;
    /// What the endpoint is sent, as JSON: Monid's `input`, with the
    /// `queryParams`, `body` or `pathParams` that `monid inspect` lists.
    /// `$query` stands for the words searched and `$language` for the
    /// book's language.
    std::string input;

    static WebSearchSettings defaults();
    bool isConfigured() const;
    /// `input` with the query and language in place, escaped for JSON.
    std::string request(const std::string& query, const std::string& language) const;

    bool operator==(const WebSearchSettings& other) const {
        return apiKey == other.apiKey && provider == other.provider && endpoint == other.endpoint && input == other.input;
    }
    bool operator!=(const WebSearchSettings& other) const { return !(*this == other); }
};

/// Reads and writes the settings file: the model settings, the web search,
/// the sync account and the reading style. There is no keychain on a Kindle, so the tokens
/// and the password sit in the same file, which is also the easiest place
/// to type them from a computer.
class SettingsStore {
public:
    explicit SettingsStore(std::string path);

    AiSettings ai() const;
    void saveAi(const AiSettings& settings);
    WebSearchSettings webSearch() const;
    void saveWebSearch(const WebSearchSettings& settings);
    SyncSettings sync() const;
    void saveSync(const SyncSettings& settings);
    ReadingStyle style() const;
    void saveStyle(const ReadingStyle& style);
    /// Whether a page turn is played as a turn or the next page just appears.
    bool animatesTurns() const;
    void saveAnimatesTurns(bool animates);

    /// Overrides for this run only, from the command line; nothing is written.
    void overrideForRun(const std::string& endpoint, const std::string& model);

private:
    std::string path_;
    std::string runEndpoint_;
    std::string runModel_;
};
