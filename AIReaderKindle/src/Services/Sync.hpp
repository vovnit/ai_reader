#pragma once

#include "Domain/Sync/SyncParts.hpp"
#include "Env.hpp"
#include "SyncStore.hpp"

#include <string>
#include <vector>

/// One round of syncing the records, after the books (`LibrarySync`): find
/// the files in `aireader-sync` that are due, read them, merge this device's
/// records in and write the result here, send the files the merge changed,
/// and remember where each file stands (`SyncParts`). The network parts run
/// on a worker thread; the database parts on the main loop.
namespace Sync {

/// What this device has, taken before the round starts.
struct Local {
    SyncDocument records;
    std::vector<SyncParts::File> known;
};

Local gather(Env& env, const SyncSettings& settings);

/// What the server has of the due files.
struct Fetched {
    /// One part per due file, with no records when the server has none.
    std::vector<SyncParts::Part> remote;
    /// Every file in the folder, with its version.
    std::vector<SyncParts::File> listed;
    /// The old single file's records and version, when it has changed since
    /// this device last read it.
    SyncDocument old;
    std::string oldVersion;
};

/// Lists the folder, reads the old file if it changed, then the due files.
/// Blocking; throws `WebDav::Error`.
Fetched fetch(const SyncSettings& settings, const Local& local);

struct Round {
    /// Due files the merge changed, to be sent.
    std::vector<SyncParts::Part> outgoing;
    /// The due files as this device knows them once those are sent.
    std::vector<SyncParts::File> settled;
    std::string oldVersion;
    SyncStore::Applied applied;
};

/// Merges the due files with this device's records and writes what changed
/// here.
Round reconcile(Env& env, const Fetched& fetched);

struct Sent {
    /// The files sent, with the versions the server gave them.
    std::vector<SyncParts::File> files;
    /// Why it stopped early.
    std::string error;
};

/// Sends the files the merge changed. Blocking.
Sent send(const SyncSettings& settings, const Round& round);

/// Writes down what this device now knows of the files. One that could not
/// be sent is left as it was known, so the next sync takes it up again.
void record(Env& env, const SyncSettings& settings, const Round& round, const Sent& sent);

struct Report {
    /// Books sent to the server.
    int sent = 0;
    SyncStore::Applied applied;
    bool uploaded = false;

    std::string summary() const;
};

}  // namespace Sync
