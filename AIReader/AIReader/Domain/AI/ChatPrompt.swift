import Foundation

/// One turn of a conversation.
struct ChatTurn: Equatable, Identifiable, Sendable {
    let id: UUID
    let isReader: Bool
    var text: String
}

/// Builds the conversation sent to the model. What it is about — the page on
/// screen, a word just explained, what the book says about a name — goes in
/// once, with the first question.
enum ChatPrompt {
    static let system = """
        Ты помогаешь читателю разобраться с книгой на иностранном языке. \
        Отвечай по-русски, коротко и по делу. Можно объяснять грамматику, разбирать \
        предложения, пересказывать содержание и отвечать на вопросы о тексте.

        Инструмент search_book ищет слово или фразу в тексте книги — до места, до которого \
        читатель дочитал, — и в других книгах той же серии. Пользуйся им, когда вопрос о том, \
        что было раньше: о персонаже, месте, событии, о том, где слово уже встречалось. \
        Не пересказывай того, чего читатель ещё не читал.
        """

    /// What the conversation starts from, as the model reads it.
    static func pageContext(_ page: String) -> String {
        "Страница:\n\(page)"
    }

    static func wordContext(word: String, sentence: String, explanation: WordExplanation) -> String {
        var context = "Слово: \(word)\nПредложение: \(sentence)\nНачальная форма: \(explanation.lemma)"
        if !explanation.formNote.isEmpty { context += "\nФорма: \(explanation.formNote)" }
        context += "\nОбъяснение: \(explanation.meaning)"
        if explanation.guessed { context += "\n(Объяснение — догадка, словарной статьи не было.)" }
        return context
    }

    static func xrayContext(term: String, answer: String) -> String {
        "Термин из книги: \(term)\nЧто о нём известно по книге: \(answer)"
    }

    static func messages(context: String, turns: [ChatTurn]) -> [ChatMessage] {
        var messages: [ChatMessage] = [.system(system)]
        // The context goes in once, attached to the first question.
        for (index, turn) in turns.enumerated() {
            guard turn.isReader else {
                messages.append(ChatMessage(role: "assistant", content: turn.text))
                continue
            }
            messages.append(.user(index == 0 ? "\(context)\n\nВопрос: \(turn.text)" : turn.text))
        }
        return messages
    }
}
