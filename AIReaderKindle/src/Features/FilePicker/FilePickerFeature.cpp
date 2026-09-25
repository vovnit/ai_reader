#include "FilePickerFeature.hpp"

#include "../../Support/Files.hpp"

FilePickerFeature::FilePickerFeature(std::string root, std::function<bool(const std::string&)> accepts)
    : directory_(std::move(root)), accepts_(std::move(accepts)) {
    read();
}

bool FilePickerFeature::canGoUp() const {
    return Files::directoryName(directory_) != directory_;
}

void FilePickerFeature::enter(const Entry& folder) {
    if (!folder.isFolder) return;
    directory_ = folder.path;
    reload();
}

void FilePickerFeature::up() {
    if (!canGoUp()) return;
    directory_ = Files::directoryName(directory_);
    reload();
}

void FilePickerFeature::reload() {
    read();
    if (onChange) onChange();
}

void FilePickerFeature::read() {
    std::vector<Entry> folders, files;
    for (const auto& name : Files::list(directory_)) {
        if (name.empty() || name[0] == '.') continue;
        Entry entry{name, Files::join(directory_, name), false};
        if (Files::isDirectory(entry.path)) {
            entry.isFolder = true;
            folders.push_back(entry);
        } else if (accepts_(entry.path)) {
            files.push_back(entry);
        }
    }
    entries_ = std::move(folders);
    entries_.insert(entries_.end(), files.begin(), files.end());
}
