import Foundation
import SQLiteData

/// The database as a sync document, and a sync document written back into
/// the database.
enum SyncStore {
    /// Everything this device knows, as records.
    static func export(_ db: Database) throws -> SyncDocument {
        let books = try Book.all.fetchAll(db)
        let groups = Dictionary(
            try BookGroup.all.fetchAll(db).map { ($0.id, $0.name) },
            uniquingKeysWith: { first, _ in first }
        )
        let keys = Dictionary(books.map { ($0.id, $0.key) }, uniquingKeysWith: { first, _ in first })
        let practice = Dictionary(
            try CardPractice.all.fetchAll(db).map { ($0.lookupID, $0) },
            uniquingKeysWith: { first, _ in first }
        )

        var document = SyncDocument()
        document.books = books.map { book in
            SyncDocument.BookRecord(
                key: book.key,
                title: book.title,
                author: book.author,
                language: book.language,
                group: book.groupID.flatMap { groups[$0] },
                chapter: book.place?.chapter,
                fraction: book.place?.fraction,
                snippet: book.place?.snippet,
                // A book never read here has nothing to say about its place,
                // so any other device's record outranks it.
                updatedAt: book.updatedAt.map(SyncDocument.stamp) ?? ""
            )
        }
        document.lookups = try Lookup.all.fetchAll(db).map { lookup in
            let practised = practice[lookup.id]
            return SyncDocument.LookupRecord(
                word: lookup.word,
                sentence: lookup.sentence,
                lemma: lookup.lemma,
                formNote: lookup.formNote,
                meaning: lookup.meaning,
                language: lookup.language,
                book: lookup.bookID.flatMap { keys[$0] },
                guessed: lookup.guessed,
                confidence: lookup.confidence,
                lookedUpAt: SyncDocument.stamp(lookup.lookedUpAt),
                correct: practised?.correct ?? 0,
                wrong: practised?.wrong ?? 0,
                practicedAt: practised.map { SyncDocument.stamp($0.practicedAt) },
                updatedAt: SyncDocument.stamp(max(lookup.lookedUpAt, practised?.practicedAt ?? lookup.lookedUpAt))
            )
        }
        document.lookups += try LookupTombstone.all.fetchAll(db).map { tombstone in
            SyncDocument.LookupRecord(
                word: tombstone.word,
                sentence: tombstone.sentence,
                updatedAt: SyncDocument.stamp(tombstone.deletedAt),
                deleted: true
            )
        }
        return document.sorted
    }

    /// What `apply` changed.
    struct Applied: Equatable, Sendable {
        var books = 0
        var lookups = 0
    }

    /// Writes the merged document in: only the records that differ from
    /// what this device already has.
    static func apply(_ merged: SyncDocument, to db: Database) throws -> Applied {
        let local = try export(db)
        var applied = Applied()

        let localBooks = Dictionary(local.books.map { ($0.key, $0) }, uniquingKeysWith: { first, _ in first })
        let bookIDs = Dictionary(
            try Book.all.fetchAll(db).map { ($0.key, $0.id) },
            uniquingKeysWith: { first, _ in first }
        )
        for record in merged.books {
            guard let bookID = bookIDs[record.key], localBooks[record.key] != record else { continue }
            try apply(record, to: bookID, in: db)
            applied.books += 1
        }

        let localLookups = Dictionary(local.lookups.map { ($0.key, $0) }, uniquingKeysWith: { first, _ in first })
        for record in merged.lookups where localLookups[record.key] != record {
            try apply(record, bookIDs: bookIDs, in: db)
            applied.lookups += 1
        }

        // A group nothing is left in has been dissolved somewhere.
        let inUse = try Book.select { $0.groupID }.fetchAll(db).compactMap { $0 }
        try BookGroup.delete().where { !$0.id.in(inUse) }.execute(db)
        return applied
    }

    private static func apply(_ record: SyncDocument.BookRecord, to bookID: Book.ID, in db: Database) throws {
        var groupID: BookGroup.ID?
        if let name = record.group {
            if let existing = try BookGroup.where { $0.name.eq(name) }.fetchOne(db) {
                groupID = existing.id
            } else {
                let inserted = try BookGroup.insert { BookGroup.Draft(name: name) }.returning { $0.id }.fetchOne(db)
                groupID = inserted
            }
        }
        let current = try Book.where { $0.id.eq(bookID) }.fetchOne(db)
        let place = record.place
        // A place from elsewhere is worked out into an offset when the book
        // is next opened; the same place again is left alone.
        let pending = place != nil && place != current?.place
        let updatedAt = SyncDocument.date(record.updatedAt)
        try Book
            .update {
                $0.groupID = #bind(groupID)
                $0.updatedAt = #bind(updatedAt)
                if pending {
                    $0.place = #bind(place, as: ReadingPlace?.JSONRepresentation.self)
                    $0.placeIsPending = true
                }
            }
            .where { $0.id.eq(bookID) }
            .execute(db)
    }

    private static func apply(
        _ record: SyncDocument.LookupRecord,
        bookIDs: [String: Book.ID],
        in db: Database
    ) throws {
        let existing = try Lookup
            .where { $0.word.eq(record.word).and($0.sentence.eq(record.sentence)) }
            .fetchOne(db)
        let deletedAt = SyncDocument.date(record.updatedAt) ?? Date()

        if record.deleted {
            if let existing {
                try Lookup.delete().where { $0.id.eq(existing.id) }.execute(db)
            }
            try LookupTombstone
                .insert {
                    LookupTombstone(word: record.word, sentence: record.sentence, deletedAt: deletedAt)
                } onConflict: { ($0.word, $0.sentence) } doUpdate: { row, excluded in
                    row.deletedAt = excluded.deletedAt
                }
                .execute(db)
            return
        }

        try LookupTombstone
            .delete()
            .where { $0.word.eq(record.word).and($0.sentence.eq(record.sentence)) }
            .execute(db)

        let lookedUpAt = SyncDocument.date(record.lookedUpAt) ?? deletedAt
        // A book this device does not have leaves the lookup attached to
        // whatever it was attached to here.
        let bookID = record.book.flatMap { bookIDs[$0] } ?? existing?.bookID
        let language = record.language ?? existing?.language
        let id: Lookup.ID
        if let existing {
            id = existing.id
            try Lookup
                .update {
                    $0.lemma = record.lemma
                    $0.formNote = record.formNote
                    $0.meaning = record.meaning
                    $0.language = language
                    $0.bookID = bookID
                    $0.guessed = record.guessed
                    $0.confidence = record.confidence
                    $0.lookedUpAt = lookedUpAt
                }
                .where { $0.id.eq(id) }
                .execute(db)
        } else {
            let inserted = try Lookup
                .insert {
                    Lookup.Draft(
                        word: record.word,
                        sentence: record.sentence,
                        lemma: record.lemma,
                        formNote: record.formNote,
                        meaning: record.meaning,
                        language: language,
                        bookID: bookID,
                        guessed: record.guessed,
                        confidence: record.confidence,
                        lookedUpAt: lookedUpAt
                    )
                }
                .returning { $0.id }
                .fetchOne(db)
            guard let inserted else { return }
            id = inserted
        }

        guard record.correct + record.wrong > 0 || record.practicedAt != nil else { return }
        try CardPractice
            .insert {
                CardPractice(
                    lookupID: id,
                    correct: record.correct,
                    wrong: record.wrong,
                    practicedAt: SyncDocument.date(record.practicedAt) ?? lookedUpAt
                )
            } onConflict: { $0.lookupID } doUpdate: { row, excluded in
                row.correct = excluded.correct
                row.wrong = excluded.wrong
                row.practicedAt = excluded.practicedAt
            }
            .execute(db)
    }
}
