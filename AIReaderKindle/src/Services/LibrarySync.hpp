#pragma once

#include "Env.hpp"

#include <set>
#include <string>
#include <utility>
#include <vector>

/// The books themselves, shared as EPUB files in the `Books` folder beside
/// the sync file. A file there that this device has not met is fetched into
/// the books folder; a book here that has no file there yet is sent. The
/// browser extension saves web pages into the same folder.
///
/// Removing a book removes it from this device only: the file stays for the
/// others, and is not fetched again since this device has met it.
///
/// Like `Sync`, in three steps: what this device has is read on the main
/// loop, the files go back and forth on a worker, and what came of it is
/// written down on the main loop again.
namespace LibrarySync {

struct Local {
    std::vector<Book> books;
    std::set<std::string> met;
};

struct Outcome {
    /// Files fetched, by name on the server, and where each was saved.
    std::vector<std::pair<std::string, std::string>> received;
    /// Books that now have a file on the server, by id, and its name.
    std::vector<std::pair<long long, std::string>> named;
    /// Every name met this time, whether or not it became a book.
    std::vector<std::string> met;
    int sent = 0;
    /// Why the exchange stopped early. What was done by then is kept, so
    /// nothing is fetched or sent twice.
    std::string error;
};

Local gather(Env& env);
/// Blocking; run it off the main loop.
Outcome exchange(const SyncSettings& settings, const Local& local);
/// Shelves what arrived and remembers the names.
void record(Env& env, const Outcome& outcome);

}  // namespace LibrarySync
