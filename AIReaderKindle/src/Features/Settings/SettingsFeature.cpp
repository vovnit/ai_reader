#include "SettingsFeature.hpp"

#include "../../Services/ChatApi.hpp"
#include "../../Support/Async.hpp"
#include "../Common/SyncRunner.hpp"

#include <algorithm>

SettingsFeature::SettingsFeature(Env& env)
    : env_(env), settings_(env.settings.ai()), web_(env.settings.webSearch()), sync_(env.settings.sync()) {}

void SettingsFeature::setEndpoint(const std::string& endpoint) {
    if (endpoint == settings_.endpoint) return;
    settings_.endpoint = endpoint;
    // A model list belongs to the endpoint it came from.
    bool hadModels = !models_.empty();
    models_.clear();
    error_.clear();
    save();
    if (hadModels && onModelsChange) onModelsChange();
}

void SettingsFeature::setToken(const std::string& token) {
    if (token == settings_.apiKey) return;
    settings_.apiKey = token;
    save();
}

void SettingsFeature::setOpenAIToken(const std::string& token) {
    if (token == settings_.openAIKey) return;
    settings_.openAIKey = token;
    save();
}

void SettingsFeature::setModel(const std::string& model) {
    if (model == settings_.model) return;
    settings_.model = model;
    save();
}

void SettingsFeature::setLanguage(const std::string& language) {
    if (language == settings_.language) return;
    settings_.language = language;
    save();
}

void SettingsFeature::loadModels() {
    if (isLoadingModels_) return;
    isLoadingModels_ = true;
    error_.clear();
    if (onChange) onChange();

    struct Loaded {
        std::vector<std::string> models;
        std::string error;
    };
    AiSettings settings = settings_;
    Async::run<Loaded>(
        [settings] {
            Loaded loaded;
            try {
                loaded.models = ChatApi::models(settings);
            } catch (const std::exception& failure) {
                loaded.error = failure.what();
            }
            return loaded;
        },
        [this](Loaded loaded) {
            isLoadingModels_ = false;
            models_ = loaded.models;
            error_ = loaded.error;
            if (!models_.empty() && std::find(models_.begin(), models_.end(), settings_.model) == models_.end()) {
                settings_.model = models_.front();
                save();
            }
            if (onModelsChange) onModelsChange();
            if (onChange) onChange();
        },
        alive_);
}

void SettingsFeature::setWeb(std::string WebSearchSettings::*field, const std::string& value) {
    if (web_.*field == value) return;
    web_.*field = value;
    env_.settings.saveWebSearch(web_);
}

void SettingsFeature::setWebToken(const std::string& token) { setWeb(&WebSearchSettings::apiKey, token); }
void SettingsFeature::setWebProvider(const std::string& provider) { setWeb(&WebSearchSettings::provider, provider); }
void SettingsFeature::setWebEndpoint(const std::string& endpoint) { setWeb(&WebSearchSettings::endpoint, endpoint); }
void SettingsFeature::setWebInput(const std::string& input) { setWeb(&WebSearchSettings::input, input); }

void SettingsFeature::setSyncUrl(const std::string& url) {
    if (url == sync_.url) return;
    sync_.url = url;
    saveSync();
}

void SettingsFeature::setSyncUser(const std::string& user) {
    if (user == sync_.username) return;
    sync_.username = user;
    saveSync();
}

void SettingsFeature::setSyncPassword(const std::string& password) {
    if (password == sync_.password) return;
    sync_.password = password;
    saveSync();
}

void SettingsFeature::syncNow() {
    if (isSyncing_ || !sync_.isConfigured()) return;
    isSyncing_ = true;
    syncMessage_.clear();
    if (onChange) onChange();
    SyncRunner::run(env_, alive_, [this](const std::string& message, bool) {
        isSyncing_ = false;
        syncMessage_ = message;
        if (onChange) onChange();
    });
}

void SettingsFeature::save() {
    env_.settings.saveAi(settings_);
    if (onChange) onChange();
}

void SettingsFeature::saveSync() {
    env_.settings.saveSync(sync_);
    if (onChange) onChange();
}
