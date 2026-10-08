#pragma once

#include "SyncDocument.hpp"

#include <string>
#include <vector>

/// The sync document kept as many small files rather than one, so that a
/// sync reads only the files another device has changed and writes only the
/// ones its own changes fall in — what keeps it quick once a library holds
/// thousands of words. A record's file follows from its key alone, the same
/// on every device: `00.json` to `ff.json` in the folder `aireader-sync`,
/// each one a `SyncDocument` of the records that belong there.
///
/// A device remembers, for each file, the version the server last gave it
/// (its ETag) and a fingerprint of its own records that belong there. A file
/// whose version or fingerprint has moved since is due: it is read, merged
/// with the records here, and written back when the merge changed it. Two
/// devices writing one file at once can overwrite each other; the one
/// overwritten finds a new version there on its next sync and writes its
/// records again.
///
/// Older versions kept everything in the one file `aireader-sync.json`. A
/// sync still reads it whenever it changes and merges it in, so the first
/// sync after an update moves a library over, and a device not yet updated
/// passes its changes on. It is never written.
///
/// Both apps compile this file, like `SyncDocument`; the web app ports it.
struct SyncParts {
    static const char* const folder;
    static const char* const oldFile;

    /// One file's records.
    struct Part {
        std::string name;
        SyncDocument records;
    };

    /// A file as the server lists it, or as this device last knew it.
    struct File {
        std::string name;
        /// The server's version of it; empty when the server did not say.
        std::string version;
        /// This device's records that belong in it, fingerprinted; empty in
        /// a listing, and when there are none.
        std::string digest;
    };

    // Named, so that Swift can make them.
    using Parts = std::vector<Part>;
    using Files = std::vector<File>;
    using Names = std::vector<std::string>;

    /// The file a record with this key belongs in.
    static std::string nameOf(const std::string& key);
    static bool isPart(const std::string& name);

    /// The records by file, in name order. A file no record falls in is left
    /// out.
    static Parts split(const SyncDocument& document);
    /// The parts' records together.
    static SyncDocument join(const Parts& parts);
    /// A fingerprint of the records, to tell later whether they changed;
    /// empty for none.
    static std::string digest(const SyncDocument& records);
    /// Each file's fingerprint of `local`, without versions.
    static Files fingerprints(const SyncDocument& local);

    /// The files a sync must read: changed or gone on the server since this
    /// device last knew them, holding records that changed here, or holding
    /// records of the old file.
    static Names due(const Files& listed, const Files& known, const SyncDocument& local, const SyncDocument& old);
    /// Each due file merged: the server's records (`remote`, one part per due
    /// file, with none when the server has none), this device's, and the old
    /// file's. In the order of `remote`.
    static Parts merge(const Parts& remote, const SyncDocument& local, const SyncDocument& old);
    /// What this device knows of the files once `settled` have been dealt
    /// with: their entries replace those in `known`, and one with neither a
    /// version nor a digest — a file that is not there and need not be — is
    /// dropped.
    static Files remember(const Files& known, const Files& settled);
};
