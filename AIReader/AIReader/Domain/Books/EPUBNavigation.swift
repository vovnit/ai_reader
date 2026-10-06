import Foundation

/// One line of a book's table of contents, as the EPUB lists it.
struct NavigationEntry: Equatable, Sendable {
    var title: String
    /// As written: relative to the navigation document, perhaps with a
    /// `#fragment` naming a place inside the file.
    var href: String
    /// 0 for a top-level entry; an entry nested under another is deeper.
    var depth: Int
}

/// Reads the table of contents out of an EPUB's navigation document: the
/// NCX of EPUB 2, or the `<nav epub:type="toc">` of EPUB 3. The same
/// reading as the Kindle and web apps' `EpubNavigation`.
enum EPUBNavigation {
    /// Whichever the document is; an NCX has an `<ncx>` root.
    static func parse(_ data: Data) -> [NavigationEntry] {
        var isNCX = false
        XMLScanner.scan(data) { name, _ in
            if name.lowercased() == "ncx" { isNCX = true }
        }
        return isNCX ? fromNCX(data) : fromNav(data)
    }

    static func fromNCX(_ data: Data) -> [NavigationEntry] {
        var entries: [NavigationEntry] = []
        // The navPoints open at this point, so a label lands on the innermost one.
        var open: [Int] = []
        var inLabel = false
        XMLScanner.scan(data) { name, attributes in
            switch name.lowercased() {
            case "navpoint":
                entries.append(NavigationEntry(title: "", href: "", depth: open.count))
                open.append(entries.count - 1)
            case "navlabel":
                inLabel = !open.isEmpty
            case "content":
                if let last = open.last { entries[last].href = attributes["src"] ?? "" }
            default:
                break
            }
        } onText: { _, text in
            if inLabel, let last = open.last { entries[last].title += text }
        } onEnd: { name in
            switch name.lowercased() {
            case "navpoint": _ = open.popLast()
            case "navlabel": inLabel = false
            default: break
            }
        }
        return tidy(entries)
    }

    static func fromNav(_ data: Data) -> [NavigationEntry] {
        // Each <nav> is read on its own; the one typed "toc" is the table of
        // contents, and failing that the first one is taken.
        var navs: [[NavigationEntry]] = []
        var isTOC: [Bool] = []
        var listDepth = 0
        var inLink = false
        XMLScanner.scan(data) { name, attributes in
            let name = name.lowercased()
            if name == "nav" {
                navs.append([])
                isTOC.append((attributes["epub:type"] ?? attributes["type"] ?? "").contains("toc"))
                listDepth = 0
                return
            }
            guard !navs.isEmpty else { return }
            switch name {
            case "ol", "ul":
                listDepth += 1
            case "li":
                navs[navs.count - 1].append(NavigationEntry(title: "", href: "", depth: max(listDepth - 1, 0)))
            case "a" where !navs[navs.count - 1].isEmpty:
                let last = navs[navs.count - 1].count - 1
                navs[navs.count - 1][last].href = attributes["href"] ?? ""
                inLink = true
            default:
                break
            }
        } onText: { _, text in
            guard inLink, let entries = navs.last, !entries.isEmpty else { return }
            navs[navs.count - 1][entries.count - 1].title += text
        } onEnd: { name in
            guard !navs.isEmpty else { return }
            switch name.lowercased() {
            case "a": inLink = false
            case "ol", "ul": listDepth = max(listDepth - 1, 0)
            default: break
            }
        }
        let toc = isTOC.firstIndex(of: true) ?? 0
        return tidy(navs.indices.contains(toc) ? navs[toc] : [])
    }

    /// Entries with a title and somewhere to go; titles on one line.
    private static func tidy(_ entries: [NavigationEntry]) -> [NavigationEntry] {
        var tidied: [NavigationEntry] = []
        for var entry in entries {
            entry.title = entry.title.trimmingCharacters(in: .whitespacesAndNewlines)
            if !entry.title.isEmpty, !entry.href.isEmpty { tidied.append(entry) }
        }
        return tidied
    }
}
