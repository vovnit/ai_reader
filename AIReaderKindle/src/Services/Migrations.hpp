#pragma once

#include "Database.hpp"

/// Creates and updates the tables that hold the library and its groups, past
/// lookups and their practice record, and the dictionary list. Changes are
/// additive, so an existing library survives an update.
namespace Migrations {

bool migrate(Database& database);

/// The current time, in the ISO form the tables store.
std::string now();

/// Lists the pack that ships with the app, so it appears beside any the
/// reader adds. A new library gets it from the first migration, and the
/// Dictionaries screen puts it back after the reader has removed it. Since
/// the first migration runs it, it may name only the columns that one makes.
bool addBundledPack(Database& database);

}  // namespace Migrations
