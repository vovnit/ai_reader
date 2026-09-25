import Foundation
import UniformTypeIdentifiers

/// Reads the share sheet's items into a passage. Most apps hand over only the
/// selected text; Safari also runs `Action.js`, whose results arrive as a
/// property list with the paragraph around the selection and the page language.
enum SharedItems {
    static func passage(from items: [NSExtensionItem]) async -> SharedPassage? {
        var selection: String?
        var paragraph: String?
        var offset: Int?
        var language: String?

        for provider in items.flatMap({ $0.attachments ?? [] }) {
            if provider.hasItemConformingToTypeIdentifier(UTType.propertyList.identifier),
               let results = await load(provider, as: UTType.propertyList) as? [String: Any],
               let page = results[NSExtensionJavaScriptPreprocessingResultsKey] as? [String: Any] {
                selection = selection ?? nonEmpty(page["selection"] as? String)
                paragraph = nonEmpty(page["paragraph"] as? String)
                offset = (page["offset"] as? NSNumber).flatMap { $0.intValue >= 0 ? $0.intValue : nil }
                language = nonEmpty(page["language"] as? String)
            } else if selection == nil, provider.hasItemConformingToTypeIdentifier(UTType.plainText.identifier) {
                selection = nonEmpty(text(await load(provider, as: UTType.plainText)))
            }
        }

        guard let selection else { return nil }
        return SharedPassage(selection: selection, paragraph: paragraph, offset: offset, language: language)
    }

    private static func load(_ provider: NSItemProvider, as type: UTType) async -> (any NSSecureCoding)? {
        try? await provider.loadItem(forTypeIdentifier: type.identifier)
    }

    /// Plain text arrives as a string from most apps, as bytes from some.
    private static func text(_ item: (any NSSecureCoding)?) -> String? {
        switch item {
        case let string as String: string
        case let attributed as NSAttributedString: attributed.string
        case let data as Data: String(data: data, encoding: .utf8)
        default: nil
        }
    }

    private static func nonEmpty(_ string: String?) -> String? {
        string.flatMap { $0.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty ? nil : $0 }
    }
}
