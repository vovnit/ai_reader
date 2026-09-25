#pragma once

#include "Database.hpp"

/// Creates and updates the tables that hold the library and its groups, past
/// lookups and their practice record, and the dictionary list. Changes are
/// additive, so an existing library survives an update.
namespace Migrations {

bool migrate(Database& database);

/// The current time, in the ISO form the tables store.
std::string now();

}  // namespace Migrations
