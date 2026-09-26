#pragma once

#include "Domain/Sync/SyncDocument.hpp"
#include "../../Services/Env.hpp"
#include "../../Services/Settings.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

/// Where lookups are sent: the endpoint, the token, and which of the models it
/// offers to use. Where the model may search the web. And where the app's
/// state is shared with the iOS app. Changes are saved as they are made.
class SettingsFeature {
public:
    explicit SettingsFeature(Env& env);

    const AiSettings& settings() const { return settings_; }
    const WebSearchSettings& webSearch() const { return web_; }
    const SyncSettings& sync() const { return sync_; }
    const std::vector<std::string>& models() const { return models_; }
    bool isLoadingModels() const { return isLoadingModels_; }
    bool isSyncing() const { return isSyncing_; }
    const std::string& error() const { return error_; }
    /// What the last sync did, or why it could not.
    const std::string& syncMessage() const { return syncMessage_; }

    void setEndpoint(const std::string& endpoint);
    void setToken(const std::string& token);
    void setOpenAIToken(const std::string& token);
    void setModel(const std::string& model);
    void setLanguage(const std::string& language);
    void loadModels();
    void setWebToken(const std::string& token);
    void setWebProvider(const std::string& provider);
    void setWebEndpoint(const std::string& endpoint);
    void setWebInput(const std::string& input);
    void setSyncUrl(const std::string& url);
    void setSyncUser(const std::string& user);
    void setSyncPassword(const std::string& password);
    /// Fetches the server's document, merges, applies, and sends it back.
    void syncNow();

    std::function<void()> onChange;
    /// Fired when the model list changes, which needs the picker rebuilt.
    std::function<void()> onModelsChange;

private:
    Env& env_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    AiSettings settings_;
    WebSearchSettings web_;
    SyncSettings sync_;
    std::vector<std::string> models_;
    bool isLoadingModels_ = false;
    bool isSyncing_ = false;
    std::string error_;
    std::string syncMessage_;

    void save();
    /// A field of the web search, saved when it changed.
    void setWeb(std::string WebSearchSettings::*field, const std::string& value);
    void saveSync();
};
