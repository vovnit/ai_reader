import Foundation

/// The name a book goes by across devices, since neither the row id nor the
/// file is the same on two of them: its title and author, normalized.
enum BookKey {
    static func make(title: String, author: String?) -> String {
        "\(normalize(title))|\(normalize(author ?? ""))"
    }

    private static func normalize(_ text: String) -> String {
        text.lowercased()
            .split(whereSeparator: \.isWhitespace)
            .joined(separator: " ")
    }
}

extension Book {
    var key: String { BookKey.make(title: title, author: author) }
}
