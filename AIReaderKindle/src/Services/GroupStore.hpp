#pragma once

#include "../Domain/Books/Book.hpp"
#include "Database.hpp"

#include <optional>
#include <string>
#include <vector>

/// The bookGroups table: the sets of books that are searched together.
class GroupStore {
public:
    explicit GroupStore(Database& database) : database_(database) {}

    /// Every group, by name.
    std::vector<BookGroup> all();
    std::optional<BookGroup> find(long long id);
    /// The group of that name, made if there is none yet.
    long long named(const std::string& name);
    /// Dissolves the group; its books stay on the shelf, ungrouped.
    void remove(long long id);

private:
    Database& database_;
};
