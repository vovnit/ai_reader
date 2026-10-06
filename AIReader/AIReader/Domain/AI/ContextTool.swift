import Foundation

/// The tool that lets the model read past what it was given: `expand_context`
/// returns the text just before the passage a conversation is about — the
/// sentence of a lookup, the page of a chat — or just after it. Each call
/// reads one step further, within the chapter.
enum ContextTool {
    static let toolName = "expand_context"

    enum Direction: String {
        case before
        case after
    }

    static let tool: [String: Any] = ToolSchema.function(
        name: toolName,
        description: "Возвращает текст книги, который идёт прямо перед тем местом, о котором речь (предложением "
            + "или страницей), или сразу после него. Каждый следующий вызов в ту же сторону читает дальше. "
            + "Используй, когда для понимания не хватает соседнего текста: к кому относится местоимение, "
            + "кто говорит, о чём шла речь абзацем выше.",
        argument: "direction",
        argumentDescription: "\"before\" — текст перед этим местом, \"after\" — текст после него.",
        choices: ["before", "after"]
    )

    /// The direction asked for, or nil when the model sent something else.
    static func direction(in arguments: String) -> Direction? {
        ToolSchema.argument("direction", in: arguments).flatMap(Direction.init(rawValue:))
    }

    /// What the model is told when there is no passage to read around, or it
    /// asked for neither direction.
    static let nothingAround = "There is no book text around this to read."
    static let unknownDirection = "The direction must be “before” or “after”."

    /// Reads one step past `window` in `text`, the book's text, without
    /// leaving `chapter`; widens the window by it, and says what was read as
    /// the model reads it.
    static func read(_ direction: Direction, in text: NSString, chapter: NSRange, window: inout BookPassage) -> String {
        let floor = chapter.location
        let ceiling = NSMaxRange(chapter)
        window.start = min(max(window.start, floor), ceiling)
        window.end = min(max(window.end, window.start), ceiling)
        switch direction {
        case .before:
            guard window.start > floor else { return "Nothing comes before it: the chapter begins there." }
            let from = BookSearch.stepBack(in: text, from: window.start, floor: floor)
            let read = trimmed(text, from..<window.start)
            window.start = from
            return "Before it in the book:\n\(read)" + (from == floor ? "\n(The chapter begins here.)" : "")
        case .after:
            guard window.end < ceiling else { return "Nothing comes after it: the chapter ends there." }
            let to = BookSearch.stepForward(in: text, from: window.end, ceiling: ceiling)
            let read = trimmed(text, window.end..<to)
            window.end = to
            return "After it in the book:\n\(read)" + (to == ceiling ? "\n(The chapter ends here.)" : "")
        }
    }

    private static func trimmed(_ text: NSString, _ range: Range<Int>) -> String {
        text.substring(with: NSRange(location: range.lowerBound, length: range.count))
            .trimmingCharacters(in: .whitespacesAndNewlines)
    }
}
