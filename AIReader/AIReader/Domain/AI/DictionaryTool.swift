import Foundation

/// The tool that lets the model open the reader's dictionary:
/// `lookup_dictionary` finds a word's articles in the offline packs, in a
/// lookup when the first entries do not fit, and in a conversation when a
/// word comes up.
enum DictionaryTool {
    static let toolName = "lookup_dictionary"

    static let tool: [String: Any] = ToolSchema.function(
        name: toolName,
        description: "Ищет слово в офлайн-словаре и возвращает найденные статьи.",
        argument: "word",
        argumentDescription: "Форма слова, которую нужно найти."
    )

    static func word(in arguments: String) -> String {
        ToolSchema.argument("word", in: arguments) ?? ""
    }
}
