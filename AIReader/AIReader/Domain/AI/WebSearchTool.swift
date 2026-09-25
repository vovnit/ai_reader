import Foundation

/// The tool that lets the model look beyond the book and the dictionary:
/// `search_web` finds pages about a name, a place, an event or an
/// expression, and hands the model a few of them with their text.
enum WebSearchTool {
    static let toolName = "search_web"

    /// How many pages a call returns to the model.
    static let resultLimit = 5
    /// How much of one page's text the model gets.
    static let textLimit = 600

    /// One page found.
    struct WebHit: Equatable {
        var title: String
        var url: String
        var text: String
    }

    static let tool: [String: Any] = ToolSchema.function(
        name: toolName,
        description: "Ищет в интернете и возвращает несколько найденных страниц с фрагментами их текста. "
            + "Используй, когда ни словарь, ни книга не помогают: имя, место, событие, реалия, "
            + "название, сленг или выражение, которых нет в словаре. Не ищи то, что есть в словаре.",
        argument: "query",
        argumentDescription: "Что искать: короткий запрос, как в поисковой строке."
    )

    static func query(in arguments: String) -> String {
        ToolSchema.argument("query", in: arguments) ?? ""
    }

    /// The pages in a search endpoint's answer, whatever the provider: the
    /// first list of objects found, read by the usual field names — title or
    /// name, url or link, text, snippet or description.
    static func hits(in output: Any?) -> [WebHit] {
        guard let list = firstList(in: output) else { return [] }
        var found: [WebHit] = []
        for item in list {
            var hit = WebHit(
                title: field(item, ["title", "name"]),
                url: field(item, ["url", "link", "href"]),
                text: field(item, ["text", "snippet", "description", "content", "summary"])
            )
            if hit.title.isEmpty, hit.url.isEmpty, hit.text.isEmpty { continue }
            hit.text = shorten(
                hit.text.replacingOccurrences(of: "\n", with: " ").trimmingCharacters(in: .whitespaces),
                to: textLimit
            )
            found.append(hit)
            if found.count == resultLimit { break }
        }
        return found
    }

    /// The pages as the model reads them: numbered, with their address and
    /// text. When none can be read out of `output`, the model gets the
    /// answer as it came, cut to a readable length, rather than nothing.
    static func summary(query: String, output: Any?) -> String {
        let found = hits(in: output)
        if found.isEmpty {
            if isNothing(output) { return "Nothing was found on the web for “\(query)”." }
            return "The web search for “\(query)” answered:\n" + shorten(dump(output), to: 3000)
        }
        var text = "Pages found for “\(query)”:\n"
        for (index, hit) in found.enumerated() {
            text += "\(index + 1). " + (hit.title.isEmpty ? "(untitled)" : hit.title)
            if !hit.url.isEmpty { text += " — \(hit.url)" }
            text += "\n"
            if !hit.text.isEmpty { text += "   \(hit.text)\n" }
        }
        return text
    }

    // MARK: - Reading the answer

    private static func field(_ item: [String: Any], _ names: [String]) -> String {
        for name in names {
            if let value = item[name] as? String, !value.isEmpty { return value }
        }
        return ""
    }

    /// The first array of objects in `value`, searching breadth-first so
    /// `{"results": [...]}` is found before anything nested deeper.
    private static func firstList(in value: Any?) -> [[String: Any]]? {
        if let list = value as? [Any] {
            return list.first is [String: Any] ? list.compactMap { $0 as? [String: Any] } : nil
        }
        guard let object = value as? [String: Any] else { return nil }
        let members = object.sorted { $0.key < $1.key }.map(\.value)
        for member in members {
            if let list = member as? [Any], list.first is [String: Any] {
                return list.compactMap { $0 as? [String: Any] }
            }
        }
        for member in members {
            if let found = firstList(in: member) { return found }
        }
        return nil
    }

    private static func isNothing(_ output: Any?) -> Bool {
        output == nil || output is NSNull || (output as? [Any])?.isEmpty == true
    }

    private static func dump(_ output: Any?) -> String {
        guard let output,
              let data = try? JSONSerialization.data(withJSONObject: output, options: [.sortedKeys, .fragmentsAllowed]),
              let text = String(data: data, encoding: .utf8)
        else { return "\(output ?? "null")" }
        return text
    }

    private static func shorten(_ text: String, to limit: Int) -> String {
        text.count <= limit ? text : String(text.prefix(limit)) + "…"
    }
}
