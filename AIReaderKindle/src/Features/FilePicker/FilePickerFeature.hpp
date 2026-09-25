#pragma once

#include <functional>
#include <string>
#include <vector>

/// A folder at a time, for choosing one file. Only folders and the files
/// `accepts` are shown; hidden entries are not.
class FilePickerFeature {
public:
    struct Entry {
        std::string name;
        std::string path;
        bool isFolder = false;
    };

    FilePickerFeature(std::string root, std::function<bool(const std::string& path)> accepts);

    const std::string& directory() const { return directory_; }
    /// Folders first, then files, each sorted by name.
    const std::vector<Entry>& entries() const { return entries_; }
    /// Whether there is a parent folder to go to; the root of the file
    /// system is as far as it goes.
    bool canGoUp() const;

    void enter(const Entry& folder);
    void up();
    /// Rereads the folder, for when its contents may have changed.
    void reload();

    std::function<void()> onChange;

private:
    std::string directory_;
    std::function<bool(const std::string&)> accepts_;
    std::vector<Entry> entries_;

    void read();
};
