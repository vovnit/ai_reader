import AIReaderCore

/// Between the Swift records and the shared C++ ones. The C++ side has no
/// optional strings: empty means absent, which is also how the file says it.
extension SyncDocument {
    typealias Core = AIReaderCore.SyncDocument

    init(_ core: Core) {
        self.init(
            books: core.books.map { record in
                BookRecord(
                    key: String(record.key),
                    title: String(record.title),
                    author: optional(record.author),
                    language: optional(record.language),
                    group: optional(record.group),
                    chapter: record.chapter.value.map(Int.init),
                    fraction: record.fraction.value,
                    snippet: optional(record.snippet),
                    updatedAt: String(record.updatedAt)
                )
            },
            lookups: core.lookups.map { record in
                LookupRecord(
                    word: String(record.word),
                    sentence: String(record.sentence),
                    lemma: String(record.lemma),
                    formNote: String(record.formNote),
                    meaning: String(record.meaning),
                    language: optional(record.language),
                    book: optional(record.book),
                    guessed: record.guessed,
                    confidence: record.confidence,
                    lookedUpAt: String(record.lookedUpAt),
                    correct: Int(record.correct),
                    wrong: Int(record.wrong),
                    practicedAt: optional(record.practicedAt),
                    updatedAt: String(record.updatedAt),
                    deleted: record.deleted
                )
            }
        )
    }

    var core: Core {
        var core = Core()
        for book in books {
            var record = Core.BookRecord()
            record.key = std.string(book.key)
            record.title = std.string(book.title)
            record.author = std.string(book.author ?? "")
            record.language = std.string(book.language ?? "")
            record.group = std.string(book.group ?? "")
            if let chapter = book.chapter { record.chapter = .init(Int32(clamping: chapter)) }
            if let fraction = book.fraction { record.fraction = .init(fraction) }
            record.snippet = std.string(book.snippet ?? "")
            record.updatedAt = std.string(book.updatedAt)
            core.books.push_back(record)
        }
        for lookup in lookups {
            var record = Core.LookupRecord()
            record.word = std.string(lookup.word)
            record.sentence = std.string(lookup.sentence)
            record.lemma = std.string(lookup.lemma)
            record.formNote = std.string(lookup.formNote)
            record.meaning = std.string(lookup.meaning)
            record.language = std.string(lookup.language ?? "")
            record.book = std.string(lookup.book ?? "")
            record.guessed = lookup.guessed
            record.confidence = lookup.confidence
            record.lookedUpAt = std.string(lookup.lookedUpAt)
            record.correct = Int32(clamping: lookup.correct)
            record.wrong = Int32(clamping: lookup.wrong)
            record.practicedAt = std.string(lookup.practicedAt ?? "")
            record.updatedAt = std.string(lookup.updatedAt)
            record.deleted = lookup.deleted
            core.lookups.push_back(record)
        }
        return core
    }
}

private func optional(_ text: std.string) -> String? {
    text.empty() ? nil : String(text)
}
