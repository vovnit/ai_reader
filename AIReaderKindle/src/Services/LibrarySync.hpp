#pragma once

#include "Env.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

/// The books themselves, shared as EPUB files in the `Books` folder beside
/// the sync file. A sync lists the folder, so the library can show what is
/// there, and sends a book here that has no file there yet. A file is
/// fetched only when asked for. The browser extension saves web pages into
/// the same folder.
///
/// A book here without a file is matched by the name it would be given, so
/// a book added on two devices separately ends up on the server once.
///
/// Like `Sync`, in three steps: the books are read on the main loop, the
/// files go back and forth on a worker, and what came of it is written down
/// on the main loop again.
namespace LibrarySync {

struct Outcome {
    /// The folder's books, when it was listed.
    std::optional<std::vector<std::string>> listed;
    /// Files that are no longer in the folder.
    std::vector<std::string> gone;
    /// Files fetched, by name on the server, and where each was saved.
    std::vector<std::pair<std::string, std::string>> received;
    /// Books that now have a file on the server, by id, and its name.
    std::vector<std::pair<long long, std::string>> named;
    int sent = 0;
    /// Why it stopped early. What was done by then is kept, so nothing is
    /// sent twice.
    std::string error;
};

/// Lists the folder and sends the books that have no file there. Blocking;
/// run it off the main loop, as the two below.
Outcome exchange(const SyncSettings& settings, const std::vector<Book>& books);
/// Fetches one file into the books folder. A book already here that has no
/// file yet takes it as its own instead.
Outcome fetch(const SyncSettings& settings, const std::vector<Book>& books, const std::string& name);
/// Deletes one file from the folder. Devices that have the book keep their
/// copy.
Outcome remove(const SyncSettings& settings, const std::string& name);
/// Shelves what arrived and writes down the names.
void record(Env& env, const Outcome& outcome);

}  // namespace LibrarySync
