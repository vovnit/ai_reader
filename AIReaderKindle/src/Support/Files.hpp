#pragma once

#include <optional>
#include <string>
#include <vector>

/// The little of the file system the app needs, on top of GLib so paths and
/// encodings behave the same on the Kindle and on a desktop.
namespace Files {

std::optional<std::string> read(const std::string& path);
bool write(const std::string& path, const std::string& contents);
bool exists(const std::string& path);
bool isDirectory(const std::string& path);
bool ensureDirectory(const std::string& path);
bool remove(const std::string& path);
/// Copies a file in chunks, so a large book never sits in memory whole.
bool copy(const std::string& from, const std::string& to);
/// Entry names (not paths) of a directory, sorted.
std::vector<std::string> list(const std::string& directory);

std::string join(const std::string& directory, const std::string& name);
std::string baseName(const std::string& path);
std::string directoryName(const std::string& path);
/// The extension, lowercased, without the dot.
std::string extension(const std::string& path);
/// The file name without its extension.
std::string stem(const std::string& path);

}  // namespace Files
