#pragma once

#include "Domain/Sync/SyncDocument.hpp"
#include "Env.hpp"

/// The database as a sync document, and a sync document written back into
/// the database. Runs on the main loop, where the stores are used.
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

}  // namespace SyncStore
