#include "LibraryStore.hpp"

#include "../Support/Files.hpp"
#include "EpubLoader.hpp"
#include "Migrations.hpp"

#include <set>

Book LibraryStore::read(Statement& row) {
    Book book;
    book.id = row.integer(0);
    book.title = row.text(1);
    book.author = row.text(2);
    book.language = row.text(3);
    book.path = row.text(4);
    book.addedAt = row.text(5);
    book.readingChapter = static_cast<int>(row.integer(6));
    book.readingOffset = static_cast<int>(row.integer(7));
    book.groupId = row.integer(8);
    if (!row.isNull(9)) {
        ReadingPlace place;
        place.chapter = book.readingChapter;
        place.fraction = row.real(9);
        place.snippet = row.text(10);
        book.place = place;
    }
    book.placePending = row.integer(11) != 0;
    book.updatedAt = row.text(12);
    book.remoteName = row.text(13);
    return book;
}

static const char* const columns =
    "id, title, author, language, path, addedAt, readingChapter, readingOffset, IFNULL(groupID, 0),"
    " placeFraction, placeSnippet, placePending, IFNULL(updatedAt, ''), IFNULL(remoteName, '')";

std::vector<Book> LibraryStore::all() {
    std::vector<Book> books;
    Statement query(database_, std::string("SELECT ") + columns + " FROM books ORDER BY addedAt DESC, id DESC");
    while (query.step()) books.push_back(read(query));
    return books;
}

std::optional<Book> LibraryStore::find(long long id) {
    Statement query(database_, std::string("SELECT ") + columns + " FROM books WHERE id = ?");
    query.bind(1, id);
    if (!query.step()) return std::nullopt;
    return read(query);
}

std::vector<Book> LibraryStore::inGroup(long long groupId) {
    std::vector<Book> books;
    Statement query(database_, std::string("SELECT ") + columns + " FROM books WHERE groupID = ? ORDER BY addedAt, id");
    query.bind(1, groupId);
    while (query.step()) books.push_back(read(query));
    return books;
}

long long LibraryStore::add(const Book& draft) {
    Statement insert(database_,
        "INSERT INTO books (title, author, language, path, addedAt) VALUES (?, ?, ?, ?, ?)");
    insert.bind(1, draft.title).bind(2, draft.author).bind(3, draft.language)
        .bind(4, draft.path).bind(5, Migrations::now());
    if (!insert.run()) return 0;
    return database_.lastInsertId();
}

void LibraryStore::remove(long long id) {
    Statement remove(database_, "DELETE FROM books WHERE id = ?");
    remove.bind(1, id).run();
}

void LibraryStore::savePosition(long long id, int chapter, int offset, const std::optional<ReadingPlace>& place) {
    Statement update(database_,
        "UPDATE books SET readingChapter = ?, readingOffset = ?, placeFraction = ?, placeSnippet = ?,"
        " placePending = 0, updatedAt = ? WHERE id = ?");
    update.bind(1, chapter).bind(2, offset);
    if (place) update.bind(3, place->fraction).bind(4, place->snippet); else update.bindNull(3).bindNull(4);
    update.bind(5, Migrations::now()).bind(6, id).run();
}

void LibraryStore::savePendingPlace(long long id, const ReadingPlace& place, const std::string& updatedAt) {
    Statement update(database_,
        "UPDATE books SET readingChapter = ?, readingOffset = 0, placeFraction = ?, placeSnippet = ?,"
        " placePending = 1, updatedAt = ? WHERE id = ?");
    update.bind(1, place.chapter).bind(2, place.fraction).bind(3, place.snippet).bind(4, updatedAt).bind(5, id).run();
}

void LibraryStore::saveLanguage(long long id, const std::string& language) {
    Statement update(database_, "UPDATE books SET language = ? WHERE id = ?");
    update.bind(1, language).bind(2, id).run();
    // Past lookups were recorded under the wrong language; correct those too.
    Statement lookups(database_, "UPDATE lookups SET language = ? WHERE bookID = ?");
    lookups.bind(1, language).bind(2, id).run();
}

void LibraryStore::assignGroup(long long id, long long groupId, const std::string& updatedAt) {
    Statement update(database_, "UPDATE books SET groupID = ?, updatedAt = ? WHERE id = ?");
    if (groupId) update.bind(1, groupId); else update.bindNull(1);
    update.bind(2, updatedAt.empty() ? Migrations::now() : updatedAt).bind(3, id).run();
}

void LibraryStore::setRemoteName(long long id, const std::string& name) {
    Statement update(database_, "UPDATE books SET remoteName = ? WHERE id = ?");
    update.bind(1, name).bind(2, id).run();
}

std::set<std::string> LibraryStore::remoteNamesMet() {
    std::set<std::string> names;
    Statement query(database_, "SELECT name FROM remoteBooks");
    while (query.step()) names.insert(query.text(0));
    return names;
}

void LibraryStore::meetRemote(const std::string& name) {
    Statement insert(database_, "INSERT OR IGNORE INTO remoteBooks (name) VALUES (?)");
    insert.bind(1, name).run();
}

void LibraryStore::refresh(const std::vector<std::string>& folders) {
    std::set<std::string> known;
    for (const auto& book : all()) {
        if (Files::exists(book.path)) known.insert(book.path);
        else remove(book.id);
    }

    for (const auto& folder : folders) {
        for (const auto& name : Files::list(folder)) {
            std::string path = Files::join(folder, name);
            if (Files::extension(path) != "epub" || known.count(path)) continue;
            auto metadata = EpubLoader::metadata(path);
            if (!metadata) continue;  // not a readable EPUB; leave it alone
            Book draft;
            draft.title = metadata->title;
            draft.author = metadata->author;
            draft.language = metadata->language;
            draft.path = path;
            add(draft);
            known.insert(path);
        }
    }
}
