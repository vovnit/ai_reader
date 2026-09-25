#pragma once

#include "Domain/Sync/SyncDocument.hpp"
#include "Env.hpp"
#include "SyncStore.hpp"

#include <string>

/// One round of syncing: exchange the books (`LibrarySync`), then fetch the
/// document, merge this device's records in, write the result back here and
/// to the server. The network parts run on a
/// worker thread; the database parts on the main loop.
namespace Sync {

struct Report {
    /// Books fetched from the server and sent to it.
    int received = 0;
    int sent = 0;
    SyncStore::Applied applied;
    bool uploaded = false;

    std::string summary() const;
};

/// Fetches and parses the server's document; nothing when there is none
/// yet. Blocking; throws `WebDav::Error`.
SyncDocument fetch(const SyncSettings& settings);
/// Sends the document. Blocking; throws `WebDav::Error`.
void store(const SyncSettings& settings, const SyncDocument& document);

/// Merges `remote` in and writes what changed; the caller stores the result
/// when `uploaded` is set.
Report reconcile(Env& env, const SyncDocument& remote, SyncDocument& merged);

}  // namespace Sync
