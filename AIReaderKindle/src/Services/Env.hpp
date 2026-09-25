#pragma once

#include "CardStore.hpp"
#include "Database.hpp"
#include "DictionaryPacks.hpp"
#include "GroupStore.hpp"
#include "LibraryStore.hpp"
#include "LookupCache.hpp"
#include "Settings.hpp"

/// Everything a feature may reach for. Built once in `main`, handed down by
/// reference, and never touched from a worker thread.
struct Env {
    Database& database;
    LibraryStore library;
    GroupStore groups;
    LookupCache lookups;
    CardStore cards;
    DictionaryPacks packs;
    SettingsStore settings;

    explicit Env(Database& db, const std::string& settingsPath)
        : database(db), library(db), groups(db), lookups(db), cards(db, lookups), packs(db), settings(settingsPath) {}
};
