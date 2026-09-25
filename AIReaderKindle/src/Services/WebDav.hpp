#pragma once

#include "Settings.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

/// What sync needs of a WebDAV server: fetch one file, store one file, and
/// list a folder. Any server that speaks HTTP GET, PUT and PROPFIND with a
/// password will do. Blocking; run it off the main loop.
namespace WebDav {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// The file's bytes, or nothing when there is no such file yet.
std::optional<std::string> download(const std::string& url, const SyncSettings& settings);
/// Stores the file, making its folder first if the server says there is none.
void upload(const std::string& url, const std::string& contents, const SyncSettings& settings,
            const std::string& type = "application/json");
/// The names of the files in a folder, folders left out; nothing when there
/// is no such folder yet.
std::vector<std::string> list(const std::string& folderUrl, const SyncSettings& settings);

/// The file names a PROPFIND answer lists, decoded; folders are left out.
std::vector<std::string> fileNames(const std::string& multistatus);
/// A name made safe to put in a URL's path.
std::string escape(const std::string& name);

}  // namespace WebDav
