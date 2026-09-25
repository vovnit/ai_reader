import Dependencies
import Foundation
import OSLog
import SQLiteData

private let logger = Logger(subsystem: "AIReader", category: "Database")

/// Creates and migrates the database that holds the library and past lookups.
func appDatabase() throws -> any DatabaseWriter {
    @Dependency(\.context) var context

    var configuration = Configuration()
    #if DEBUG
    configuration.prepareDatabase { db in
        db.trace(options: .profile) { logger.debug("\($0.expandedDescription)") }
    }
    #endif

    #if os(iOS)
    // The extension may write while the app sleeps in the background; without
    // this the app can be killed for holding the file lock while suspended.
    configuration.observesSuspensionNotifications = true
    #endif

    try FileManager.default.createDirectory(at: AppGroup.container, withIntermediateDirectories: true)
    let database = try defaultDatabase(
        path: AppGroup.container.appending(path: "SQLiteData.db").path,
        configuration: configuration
    )
    logger.info("open '\(database.path)'")

    var migrator = DatabaseMigrator()
    #if DEBUG
    migrator.eraseDatabaseOnSchemaChange = context != .live
    #endif
    migrator.registerMigration("Create tables") { db in
        try #sql(
            """
            CREATE TABLE "books" (
              "id" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
              "title" TEXT NOT NULL,
              "author" TEXT,
              "language" TEXT,
              "folder" TEXT NOT NULL,
              "packagePath" TEXT NOT NULL,
              "coverPath" TEXT,
              "addedAt" TEXT NOT NULL,
              "readingOffset" INTEGER NOT NULL DEFAULT 0
            ) STRICT
            """
        )
        .execute(db)

        try #sql(
            """
            CREATE TABLE "lookups" (
              "id" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
              "word" TEXT NOT NULL,
              "sentence" TEXT NOT NULL,
              "lemma" TEXT NOT NULL,
              "formNote" TEXT NOT NULL,
              "meaning" TEXT NOT NULL,
              "guessed" INTEGER NOT NULL,
              "confidence" REAL NOT NULL,
              "lookedUpAt" TEXT NOT NULL
            ) STRICT
            """
        )
        .execute(db)

        try #sql(
            """
            CREATE UNIQUE INDEX "lookups_word_sentence" ON "lookups"("word", "sentence")
            """
        )
        .execute(db)
    }
    migrator.registerMigration("Add sentence translation and language") { db in
        try #sql(
            """
            ALTER TABLE "lookups" ADD COLUMN "sentenceTranslation" TEXT NOT NULL DEFAULT ''
            """
        )
        .execute(db)

        try #sql(
            """
            ALTER TABLE "lookups" ADD COLUMN "language" TEXT
            """
        )
        .execute(db)
    }

    migrator.registerMigration("Attribute lookups to a book") { db in
        try #sql(
            """
            ALTER TABLE "lookups" ADD COLUMN "bookID" INTEGER REFERENCES "books"("id") ON DELETE SET NULL
            """
        )
        .execute(db)
    }
    migrator.registerMigration("Add dictionaries") { db in
        try #sql(
            """
            CREATE TABLE "dictionaryPacks" (
              "id" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
              "name" TEXT NOT NULL,
              "fileName" TEXT,
              "targetLanguage" TEXT,
              "definitionLanguage" TEXT,
              "isEnabled" INTEGER NOT NULL DEFAULT 1,
              "addedAt" TEXT NOT NULL
            ) STRICT
            """
        )
        .execute(db)

        // The pack that ships inside the app, so it appears in the list beside
        // any the reader adds.
        try DictionaryPack.insert {
            DictionaryPack.Draft(
                name: "Bundled dictionary",
                fileName: nil,
                targetLanguage: "fr",
                definitionLanguage: "ru"
            )
        }
        .execute(db)
    }

    migrator.registerMigration("Add groups, practice, places and tombstones") { db in
        // Groups of books read together — a series, a course — so a search
        // can run across them. A book leaves its group when the group goes.
        try #sql(
            """
            CREATE TABLE "bookGroups" (
              "id" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
              "name" TEXT NOT NULL,
              "createdAt" TEXT NOT NULL
            ) STRICT
            """
        )
        .execute(db)
        try #sql(
            """
            ALTER TABLE "books" ADD COLUMN "groupID" INTEGER REFERENCES "bookGroups"("id") ON DELETE SET NULL
            """
        )
        .execute(db)

        // The reading position in the form another device can find again,
        // and when it or the group last changed, for syncing.
        try #sql(#"ALTER TABLE "books" ADD COLUMN "place" TEXT"#).execute(db)
        try #sql(#"ALTER TABLE "books" ADD COLUMN "placeIsPending" INTEGER NOT NULL DEFAULT 0"#).execute(db)
        try #sql(#"ALTER TABLE "books" ADD COLUMN "updatedAt" TEXT"#).execute(db)
        // A book read before this column existed was read at some point; its
        // arrival is the nearest date there is.
        try #sql(#"UPDATE "books" SET "updatedAt" = "addedAt" WHERE "readingOffset" > 0"#).execute(db)

        // How each flash card has fared in practice. The card is the lookup
        // itself, so the record goes when the lookup does.
        try #sql(
            """
            CREATE TABLE "cardPractice" (
              "lookupID" INTEGER PRIMARY KEY REFERENCES "lookups"("id") ON DELETE CASCADE,
              "correct" INTEGER NOT NULL DEFAULT 0,
              "wrong" INTEGER NOT NULL DEFAULT 0,
              "practicedAt" TEXT NOT NULL
            ) STRICT
            """
        )
        .execute(db)

        // Lookups that were deleted, so a sync deletes them elsewhere too.
        try #sql(
            """
            CREATE TABLE "lookupTombstones" (
              "word" TEXT NOT NULL,
              "sentence" TEXT NOT NULL,
              "deletedAt" TEXT NOT NULL,
              PRIMARY KEY ("word", "sentence")
            ) STRICT
            """
        )
        .execute(db)
    }

    migrator.registerMigration("Share books through the sync folder") { db in
        try #sql(#"ALTER TABLE "books" ADD COLUMN "remoteName" TEXT"#).execute(db)
        try #sql(
            """
            CREATE TABLE "remoteBooks" (
              "name" TEXT PRIMARY KEY NOT NULL
            ) STRICT
            """
        )
        .execute(db)
    }

    try migrator.migrate(database)

    return database
}
