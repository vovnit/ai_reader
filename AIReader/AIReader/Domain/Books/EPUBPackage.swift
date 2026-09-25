import Foundation

/// The parsed contents of an EPUB package document (the `.opf` file): what the
/// book is called, which files it is made of, and in what order they are read.
struct EPUBPackage: Equatable, Sendable {
    struct Item: Equatable, Sendable {
        let id: String
        let href: String
        let mediaType: String
        let properties: String
    }

    var title = ""
    var author: String?
    var language: String?
    var items: [String: Item] = [:]
    var spine: [String] = []
    var coverItemID: String?

    /// Items to render, in reading order, limited to markup documents.
    var readingOrder: [Item] {
        spine.compactMap { items[$0] }.filter { $0.mediaType.contains("html") }
    }

    /// The manifest item holding the cover image, either flagged by the EPUB 3
    /// `cover-image` property or pointed at by the EPUB 2 `cover` meta tag.
    var coverItem: Item? {
        if let flagged = items.values.first(where: { $0.properties.contains("cover-image") }) {
            return flagged
        }
        return coverItemID.flatMap { items[$0] }
    }

    /// Parses a package document.
    static func parse(_ data: Data) -> EPUBPackage {
        var package = EPUBPackage()
        var inSpine = false

        XMLScanner.scan(data) { name, attributes in
            switch name {
            case "item":
                guard let id = attributes["id"], let href = attributes["href"] else { return }
                package.items[id] = Item(
                    id: id,
                    href: href.removingPercentEncoding ?? href,
                    mediaType: attributes["media-type"] ?? "",
                    properties: attributes["properties"] ?? ""
                )
            case "spine":
                inSpine = true
            case "itemref":
                if inSpine, let idref = attributes["idref"] { package.spine.append(idref) }
            case "meta":
                if attributes["name"] == "cover" { package.coverItemID = attributes["content"] }
            default:
                break
            }
        } onText: { name, text in
            switch name {
            case "title" where package.title.isEmpty: package.title = text
            case "creator" where package.author == nil: package.author = text
            case "language" where package.language == nil: package.language = text
            default: break
            }
        }

        return package
    }

    /// Reads `META-INF/container.xml` to find the package document's path.
    static func packagePath(inContainer data: Data) -> String? {
        var path: String?
        XMLScanner.scan(data) { name, attributes in
            if name == "rootfile", path == nil { path = attributes["full-path"] }
        }
        return path
    }
}
