#pragma once

#include "../../Domain/Books/Book.hpp"
#include "../../Services/Env.hpp"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

/// The shelf: the books found in the book folders, the groups they are
/// sorted into, the books in the sync folder not fetched yet, and the door
/// into the reader.
class LibraryFeature {
public:
    explicit LibraryFeature(Env& env);

    const std::vector<Book>& books() const { return books_; }
    const std::vector<BookGroup>& groups() const { return groups_; }
    /// The books in a group; for 0, the books in none.
    std::vector<Book> booksIn(long long groupId) const;
    /// The files in the sync folder no book here has, by name; none when no
    /// server is set up.
    const std::vector<std::string>& cloudBooks() const { return cloud_; }
    /// Whether the book's file is in the sync folder.
    bool isInCloud(const Book& book) const;
    bool isDownloading(const std::string& name) const { return downloading_.count(name) > 0; }

    /// Scans the folders for new or missing files and reloads the list.
    void refresh();
    /// Takes the book off the shelf and deletes its file.
    void remove(const Book& book);
    /// Deletes the book's file from the sync folder, then from here.
    void removeEverywhere(const Book& book);
    /// Deletes a file from the sync folder. Devices that have the book keep
    /// their copy.
    void removeRemote(const std::string& name);
    /// Fetches a file from the sync folder onto the shelf.
    void download(const std::string& name);
    /// Copies an `.epub` from anywhere on disk into the books folder and puts
    /// it on the shelf; a `.pdf` is made into an EPUB there first. Returns
    /// what went wrong, or nothing.
    std::string add(const std::string& path);

    /// Puts the book in a group; 0 takes it out.
    void assign(const Book& book, long long groupId);
    /// The group of that name, made if there is none yet.
    long long groupNamed(const std::string& name);
    /// Dissolves the group; its books stay, ungrouped.
    void dissolve(const BookGroup& group);
    /// Brings this device in line with the others, when a server has been
    /// set up: on opening the app and on closing a book. Quiet — what came
    /// in shows on the shelf; a failure goes to the log.
    void sync();

    std::function<void()> onChange;
    /// A download or a deletion in the sync folder failed: what, and why.
    std::function<void(const std::string& title, const std::string& message)> onFailure;

private:
    Env& env_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    bool isSyncing_ = false;
    std::vector<Book> books_;
    std::vector<BookGroup> groups_;
    std::set<std::string> remote_;
    std::vector<std::string> cloud_;
    std::set<std::string> downloading_;

    void reload();
};
