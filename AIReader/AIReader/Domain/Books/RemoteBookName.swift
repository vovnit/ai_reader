import Foundation

/// The file name a book is given in the shared `Books` folder: author and
/// title, so it reads well in any file manager, with nothing a Kindle's FAT
/// partition would refuse. Written the same way in
/// `AIReaderKindle/src/Domain/Books/RemoteBookName.cpp`.
enum RemoteBookName {
    static let folder = "Books"

    /// A name none of `taken` has, compared without case, since some
    /// servers ignore it.
    static func make(title: String, author: String?, avoiding taken: Set<String>) -> String {
        let taken = Set(taken.map { $0.lowercased() })
        let base = stem(title: title, author: author)
        var name = "\(base).epub"
        var number = 2
        while taken.contains(name.lowercased()) {
            name = "\(base) (\(number)).epub"
            number += 1
        }
        return name
    }

    static func isBook(_ name: String) -> Bool {
        name.lowercased().hasSuffix(".epub") && !name.hasPrefix(".")
    }

    private static func stem(title: String, author: String?) -> String {
        let author = author?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        let raw = author.isEmpty ? title : "\(author) - \(title)"
        let forbidden = CharacterSet(charactersIn: "/\\:*?\"<>|").union(.controlCharacters)
        let cleaned = raw.unicodeScalars.map { forbidden.contains($0) ? " " : String($0) }.joined()
        var stem = cleaned.split(whereSeparator: \.isWhitespace).joined(separator: " ")
        if stem.count > 120 { stem = String(stem.prefix(120)) }
        // Windows and FAT drop a trailing dot, and a leading one hides the file.
        while stem.hasSuffix(".") || stem.hasSuffix(" ") { stem.removeLast() }
        while stem.hasPrefix(".") || stem.hasPrefix(" ") { stem.removeFirst() }
        return stem.isEmpty ? "Book" : stem
    }
}
