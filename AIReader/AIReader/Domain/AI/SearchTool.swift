import Foundation

/// The tool that lets the model read the book: `search_book` finds a phrase
/// in the book being read, and in the other books of its group.
enum SearchTool {
    static let toolName = "search_book"

    /// How many passages a call returns to the model.
    static let passageLimit = 12

    static let tool: [String: Any] = ToolSchema.function(
        name: toolName,
        description: "Ищет слово или фразу в тексте книги, которую читает пользователь (и в других книгах той же серии), "
            + "и возвращает отрывки, где она встречается, — только до места, до которого читатель дочитал.",
        argument: "query",
        argumentDescription: "Слово или короткая фраза, которую нужно найти в книге."
    )

    static func query(in arguments: String) -> String {
        ToolSchema.argument("query", in: arguments) ?? ""
    }

    /// The passages as the model reads them: numbered, each with its book when
    /// several are searched, and its chapter.
    static func summary(query: String, hits: [SearchHit], severalBooks: Bool) -> String {
        guard !hits.isEmpty else { return "No passage read so far contains “\(query)”." }
        var text = "Passages with “\(query)”:\n"
        for (index, hit) in hits.enumerated() {
            text += "\(index + 1). ["
            if severalBooks { text += "\(hit.bookTitle), " }
            text += "chapter \(hit.chapter + 1)] \(hit.excerpt)\n"
        }
        return text
    }
}
