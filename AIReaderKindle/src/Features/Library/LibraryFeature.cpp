#include "LibraryFeature.hpp"

#include "../../Services/EpubLoader.hpp"
#include "../../Services/Paths.hpp"
#include "../../Support/Files.hpp"
#include "../../Support/Text.hpp"
#include "../Common/SyncRunner.hpp"

#include <cstdio>

LibraryFeature::LibraryFeature(Env& env) : env_(env) {}

std::vector<Book> LibraryFeature::booksIn(long long groupId) const {
    std::vector<Book> books;
    for (const auto& book : books_) {
        if (book.groupId == groupId) books.push_back(book);
    }
    return books;
}

void LibraryFeature::reload() {
    books_ = env_.library.all();
    groups_ = env_.groups.all();
    if (onChange) onChange();
}

void LibraryFeature::refresh() {
    env_.library.refresh(Paths::bookFolders());
    reload();
}

void LibraryFeature::remove(const Book& book) {
    env_.library.remove(book.id);
    Files::remove(book.path);
    reload();
}

std::string LibraryFeature::add(const std::string& path) {
    std::string error;
    if (!EpubLoader::metadata(path, &error)) return error.empty() ? "Not an EPUB this app can read." : error;
    // A file already in a scanned folder is picked up where it is.
    std::string folder = Files::directoryName(path);
    bool scanned = false;
    for (const auto& known : Paths::bookFolders()) scanned = scanned || known == folder;
    if (!scanned) {
        std::string destination = Files::join(Paths::books(), Files::baseName(path));
        if (!Files::exists(destination) && !Files::copy(path, destination)) {
            return "Could not copy the book into " + Paths::books() + ".";
        }
    }
    refresh();
    return "";
}

void LibraryFeature::assign(const Book& book, long long groupId) {
    env_.library.assignGroup(book.id, groupId);
    reload();
}

long long LibraryFeature::groupNamed(const std::string& name) {
    std::string trimmed = Text::trim(name);
    return trimmed.empty() ? 0 : env_.groups.named(trimmed);
}

void LibraryFeature::dissolve(const BookGroup& group) {
    env_.groups.remove(group.id);
    reload();
}

void LibraryFeature::sync() {
    if (isSyncing_) return;
    isSyncing_ = true;
    SyncRunner::run(env_, alive_, [this](const std::string& message, bool failed) {
        isSyncing_ = false;
        if (failed) std::fprintf(stderr, "aireader: sync: %s\n", message.c_str());
        reload();
    });
}
