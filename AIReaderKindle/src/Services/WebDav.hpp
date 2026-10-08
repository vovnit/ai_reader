#pragma once

#include "Settings.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

/// What sync needs of a WebDAV server: fetch, store and delete one file, and
/// list a folder. Any server that speaks HTTP GET, PUT, DELETE and PROPFIND
/// with a password will do. Blocking; run it off the main loop.
namespace WebDav {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// A file in a folder, with the version the server gives it (its ETag,
/// empty when it gives none).
struct Entry {
    std::string name;
    std::string version;
};

/// The file's bytes, or nothing when there is no such file yet.
std::optional<std::string> download(const std::string& url, const SyncSettings& settings);
/// Stores the file, making its folder first if the server says there is
/// none. Returns the version the server gives it, or nothing.
std::string upload(const std::string& url, const std::string& contents, const SyncSettings& settings,
                   const std::string& type = "application/json");
/// Deletes the file; one already gone counts as deleted.
void remove(const std::string& url, const SyncSettings& settings);
/// The files in a folder, folders left out; nothing when there is no such
/// folder yet. Given a file, the file alone.
std::vector<Entry> list(const std::string& url, const SyncSettings& settings);

/// The files a PROPFIND answer lists, decoded; folders are left out.
std::vector<Entry> entries(const std::string& multistatus);
/// A name made safe to put in a URL's path.
std::string escape(const std::string& name);

}  // namespace WebDav
