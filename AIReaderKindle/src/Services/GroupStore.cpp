#include "GroupStore.hpp"

#include "Migrations.hpp"

std::vector<BookGroup> GroupStore::all() {
    std::vector<BookGroup> groups;
    Statement query(database_, "SELECT id, name FROM bookGroups ORDER BY name COLLATE NOCASE, id");
    while (query.step()) groups.push_back({query.integer(0), query.text(1)});
    return groups;
}

std::optional<BookGroup> GroupStore::find(long long id) {
    Statement query(database_, "SELECT id, name FROM bookGroups WHERE id = ?");
    query.bind(1, id);
    if (!query.step()) return std::nullopt;
    return BookGroup{query.integer(0), query.text(1)};
}

long long GroupStore::named(const std::string& name) {
    Statement existing(database_, "SELECT id FROM bookGroups WHERE name = ?");
    existing.bind(1, name);
    if (existing.step()) return existing.integer(0);
    Statement insert(database_, "INSERT INTO bookGroups (name, createdAt) VALUES (?, ?)");
    insert.bind(1, name).bind(2, Migrations::now());
    return insert.run() ? database_.lastInsertId() : 0;
}

void GroupStore::remove(long long id) {
    Statement remove(database_, "DELETE FROM bookGroups WHERE id = ?");
    remove.bind(1, id).run();
}
