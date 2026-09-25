#pragma once

#include "../Domain/Books/Book.hpp"
#include "Database.hpp"

#include <optional>
#include <set>
#include <string>
#include <vector>

/// The books table: what is on the shelf and how far each has been read.
class LibraryStore {
public:
    explicit LibraryStore(Database& database) : database_(database) {}

    /// Every book, newest first.
    std::vector<Book> all();
    std::optional<Book> find(long long id);
    /// The books of one group, oldest first: the order they were shelved in.
    std::vector<Book> inGroup(long long groupId);
    long long add(const Book& draft);
    void remove(long long id);
    /// Where the reader is, as an offset for this device and as a place any
    /// device can find again.
    void savePosition(long long id, int chapter, int offset, const std::optional<ReadingPlace>& place);
    /// A place from another device, to be worked out into an offset when
    /// the book is next opened.
    void savePendingPlace(long long id, const ReadingPlace& place, const std::string& updatedAt);
    void saveLanguage(long long id, const std::string& language);
    /// Puts the book in a group; 0 takes it out of any. `updatedAt` is now
    /// unless the change came from a sync, which says when it was made.
    void assignGroup(long long id, long long groupId, const std::string& updatedAt = "");

    /// Records the book's file in the sync folder.
    void setRemoteName(long long id, const std::string& name);
    /// Every file in the sync folder this device has met: fetched, sent, or
    /// found to be a book it already had.
    std::set<std::string> remoteNamesMet();
    void meetRemote(const std::string& name);

    /// Brings the table in line with the folders: new files are added, files
    /// that have gone are dropped.
    void refresh(const std::vector<std::string>& folders);

private:
    Database& database_;

    static Book read(Statement& row);
};
