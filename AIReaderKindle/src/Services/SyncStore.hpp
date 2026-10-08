#pragma once

#include "Domain/Sync/SyncParts.hpp"
#include "Env.hpp"

#include <string>
#include <vector>

/// The database as a sync document, and a sync document written back into
/// the database; and what this device knows of the files on the server.
/// Runs on the main loop, where the stores are used.
namespace SyncStore {

/// Everything this device knows, as records.
SyncDocument exportAll(Env& env);

/// What `apply` changed.
struct Applied {
    int books = 0;
    int lookups = 0;
};

/// Writes the merged document in: only the records that differ from what
/// this device already has.
Applied apply(Env& env, const SyncDocument& merged);

/// What this device last knew of the files in the folder at `folder`.
std::vector<SyncParts::File> known(Env& env, const std::string& folder);
/// Replaces it, and forgets any other folder's.
void remember(Env& env, const std::string& folder, const std::vector<SyncParts::File>& files);

}  // namespace SyncStore
