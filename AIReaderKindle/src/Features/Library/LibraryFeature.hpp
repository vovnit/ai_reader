#pragma once

#include "../../Domain/Books/Book.hpp"
#include "../../Services/Env.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

/// The shelf: the books found in the book folders, the groups they are
/// sorted into, and the door into the reader.
class LibraryFeature {
public:
    explicit LibraryFeature(Env& env);

    const std::vector<Book>& books() const { return books_; }
    const std::vector<BookGroup>& groups() const { return groups_; }
    /// The books in a group; for 0, the books in none.
    std::vector<Book> booksIn(long long groupId) const;

    /// Scans the folders for new or missing files and reloads the list.
    void refresh();
    /// Takes the book off the shelf and deletes its file.
    void remove(const Book& book);
    /// Copies an `.epub` from anywhere on disk into the books folder and puts
    /// it on the shelf. Returns what went wrong, or nothing.
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

private:
    Env& env_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    bool isSyncing_ = false;
    std::vector<Book> books_;
    std::vector<BookGroup> groups_;

    void reload();
};
