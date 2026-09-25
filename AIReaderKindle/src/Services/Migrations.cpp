#include "Migrations.hpp"

#include <glib.h>

namespace Migrations {

std::string now() {
    GDateTime* time = g_date_time_new_now_utc();
    gchar* formatted = g_date_time_format(time, "%Y-%m-%dT%H:%M:%SZ");
    std::string result = formatted;
    g_free(formatted);
    g_date_time_unref(time);
    return result;
}

bool migrate(Database& database) {
    if (!database.isOpen()) return false;
    int version = database.userVersion();

    if (version < 1) {
        bool ok = database.exec(
            "CREATE TABLE books ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
            "  title TEXT NOT NULL,"
            "  author TEXT,"
            "  language TEXT,"
            "  path TEXT NOT NULL UNIQUE,"
            "  addedAt TEXT NOT NULL,"
            "  readingChapter INTEGER NOT NULL DEFAULT 0,"
            "  readingOffset INTEGER NOT NULL DEFAULT 0"
            ")")
            && database.exec(
            "CREATE TABLE lookups ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
            "  word TEXT NOT NULL,"
            "  sentence TEXT NOT NULL,"
            "  lemma TEXT NOT NULL,"
            "  formNote TEXT NOT NULL,"
            "  meaning TEXT NOT NULL,"
            "  language TEXT,"
            "  bookID INTEGER REFERENCES books(id) ON DELETE SET NULL,"
            "  guessed INTEGER NOT NULL,"
            "  confidence REAL NOT NULL,"
            "  lookedUpAt TEXT NOT NULL"
            ")")
            && database.exec("CREATE UNIQUE INDEX lookups_word_sentence ON lookups(word, sentence)")
            && database.exec(
            "CREATE TABLE dictionaryPacks ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
            "  name TEXT NOT NULL,"
            "  fileName TEXT,"
            "  targetLanguage TEXT,"
            "  definitionLanguage TEXT,"
            "  isEnabled INTEGER NOT NULL DEFAULT 1,"
            "  addedAt TEXT NOT NULL"
            ")");
        if (!ok) return false;

        // The pack that ships with the app, so it appears in the list beside
        // any the reader adds.
        Statement insert(database,
            "INSERT INTO dictionaryPacks (name, fileName, targetLanguage, definitionLanguage, addedAt)"
            " VALUES (?, NULL, ?, ?, ?)");
        insert.bind(1, "Bundled dictionary").bind(2, "fr").bind(3, "ru").bind(4, now());
        if (!insert.run()) return false;
        database.setUserVersion(1);
    }

    if (version < 2) {
        // How each flash card has fared in practice. The card is the lookup
        // itself, so the record goes when the lookup does.
        bool ok = database.exec(
            "CREATE TABLE cardPractice ("
            "  lookupID INTEGER PRIMARY KEY REFERENCES lookups(id) ON DELETE CASCADE,"
            "  correct INTEGER NOT NULL DEFAULT 0,"
            "  wrong INTEGER NOT NULL DEFAULT 0,"
            "  practicedAt TEXT NOT NULL"
            ")");
        if (!ok) return false;
        database.setUserVersion(2);
    }

    if (version < 3) {
        // Groups of books read together — a series, a course — so a search
        // can run across them. A book leaves its group when the group goes.
        bool ok = database.exec(
            "CREATE TABLE bookGroups ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
            "  name TEXT NOT NULL,"
            "  createdAt TEXT NOT NULL"
            ")")
            && database.exec("ALTER TABLE books ADD COLUMN groupID INTEGER REFERENCES bookGroups(id) ON DELETE SET NULL");
        if (!ok) return false;
        database.setUserVersion(3);
    }
    if (version < 4) {
        // The reading position in the form another device can find again,
        // when it or the group last changed, and the lookups that were
        // deleted — all for syncing with the iOS app.
        bool ok = database.exec("ALTER TABLE books ADD COLUMN placeFraction REAL")
            && database.exec("ALTER TABLE books ADD COLUMN placeSnippet TEXT")
            && database.exec("ALTER TABLE books ADD COLUMN placePending INTEGER NOT NULL DEFAULT 0")
            && database.exec("ALTER TABLE books ADD COLUMN updatedAt TEXT")
            // A book read before this column existed was read at some point;
            // its arrival is the nearest date there is.
            && database.exec("UPDATE books SET updatedAt = addedAt WHERE readingChapter > 0 OR readingOffset > 0")
            && database.exec(
            "CREATE TABLE lookupTombstones ("
            "  word TEXT NOT NULL,"
            "  sentence TEXT NOT NULL,"
            "  deletedAt TEXT NOT NULL,"
            "  PRIMARY KEY (word, sentence)"
            ")");
        if (!ok) return false;
        database.setUserVersion(4);
    }
    if (version < 5) {
        // Books shared as files in the sync folder's `Books`: the name each
        // book has there, and every name this device has met, so a book
        // removed here is not fetched again.
        bool ok = database.exec("ALTER TABLE books ADD COLUMN remoteName TEXT")
            && database.exec("CREATE TABLE remoteBooks (name TEXT PRIMARY KEY NOT NULL)");
        if (!ok) return false;
        database.setUserVersion(5);
    }
    return true;
}

}  // namespace Migrations
