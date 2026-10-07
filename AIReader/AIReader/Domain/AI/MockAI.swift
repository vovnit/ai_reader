import Foundation

/// Stands in for the model when the endpoint is `AISettings.mockEndpoint`.
///
/// It reads the same conversation a real model would and answers from the
/// material in it, so a mocked run still exercises the prompts, the
/// tool-calling loop and the JSON parsing: a lookup is answered from the
/// dictionary summary, a conversation asked to find something searches the
/// book, an X-ray counts the passages it was given.
enum MockAI {
    static let models = ["mock-medium", "mock-small"]

    static func reply(to messages: [ChatMessage]) -> ChatMessage {
        // Which prompt built this: a glossary's system prompt says so, the
        // X-ray names a term, a conversation ends its first message with a
        // question, a lookup names a word.
        if messages.contains(where: { $0.role == "system" && $0.content?.hasPrefix("Ты составляешь словарик") == true }) {
            return glossary(messages)
        }
        if let term = value(after: "Термин: ", in: messages) { return xray(term, messages) }
        if firstUserMessage(in: messages)?.contains("\nВопрос: ") == true { return chat(messages) }
        if let word = value(after: "Слово: ", in: messages) { return lookup(word, messages) }
        return chat(messages)
    }

    // MARK: - Answers

    /// A glossary batch: every numbered word defined from nothing but its spelling.
    private static func glossary(_ messages: [ChatMessage]) -> ChatMessage {
        let words: [[String: Any]] = (firstUserMessage(in: messages) ?? "")
            .split(separator: "\n")
            .compactMap { line in
                let digits = line.prefix { $0.isASCII && $0.isNumber }
                guard let number = Int(digits), line.dropFirst(digits.count).hasPrefix(". ") else { return nil }
                let spelling = String(line.dropFirst(digits.count + 2))
                guard !spelling.isEmpty else { return nil }
                return ["n": number, "lemma": spelling.lowercased(), "form_note": "", "meaning": "«\(spelling)» в книге (макет)"]
            }
        let json = (try? JSONSerialization.data(withJSONObject: ["words": words])).flatMap {
            String(data: $0, encoding: .utf8)
        }
        return ChatMessage(role: "assistant", content: json)
    }

    /// A word lookup: answer from the dictionary material in the conversation.
    private static func lookup(_ word: String, _ messages: [ChatMessage]) -> ChatMessage {
        let text = everything(in: messages)
        let article = self.article(in: text)
        let form = self.form(in: text)

        // With nothing to go on, ask the dictionary once more — the same move a
        // real model makes when the first entries are unusable.
        if article == nil, !sawToolResult(in: messages) {
            return call(DictionaryTool.toolName, id: "mock-call-1", argument: "word", value: form?.lemma ?? word)
        }

        let explanation = WordExplanation(
            lemma: article?.lemma ?? form?.lemma ?? word,
            formNote: form.map { "«\(word)» — форма слова «\($0.lemma)» (\($0.grammar))." }
                ?? "«\(word)» — начальная форма.",
            meaning: article?.firstSense ?? "Значение в словаре не найдено (макет).",
            guessed: article == nil,
            confidence: article == nil ? 0.35 : 0.9
        )

        let json = (try? JSONEncoder().encode(explanation)).flatMap {
            String(data: $0, encoding: .utf8)
        }
        return ChatMessage(role: "assistant", content: json)
    }

    /// A conversation: echo the question; asked to find something, search the
    /// book for it, asked to look something up online, search the web, asked
    /// what a word means, open the dictionary, and asked what came before or
    /// comes after, read around the passage — the way a real model would.
    private static func chat(_ messages: [ChatMessage]) -> ChatMessage {
        var question = messages.last { $0.role == "user" }?.content ?? ""
        if let marker = question.range(of: "Вопрос: ", options: .backwards) {
            question = String(question[marker.upperBound...])
        }
        let lowered = question.lowercased()
        for verb in ["найди ", "find "] where lowered.hasPrefix(verb) {
            let query = question.dropFirst(verb.count).trimmingCharacters(in: .whitespaces)
            if !sawToolResult(in: messages) {
                return call(SearchTool.toolName, id: "mock-search-1", argument: "query", value: query)
            }
            let count = passageCount(in: everything(in: messages))
            return ChatMessage(role: "assistant", content: "Макет: по запросу «\(query)» в книге нашлось \(count) отрывков.")
        }
        for verb in ["поищи ", "search "] where lowered.hasPrefix(verb) {
            let query = question.dropFirst(verb.count).trimmingCharacters(in: .whitespaces)
            if !sawToolResult(in: messages) {
                return call(WebSearchTool.toolName, id: "mock-web-1", argument: "query", value: query)
            }
            let count = passageCount(in: everything(in: messages))
            return ChatMessage(role: "assistant", content: "Макет: в интернете по запросу «\(query)» нашлось \(count) страниц.")
        }
        for verb in ["что значит ", "what does "] where lowered.hasPrefix(verb) {
            var word = question.dropFirst(verb.count).trimmingCharacters(in: .whitespaces)
            while word.hasSuffix("?") || word.hasSuffix(".") { word.removeLast() }
            if !sawToolResult(in: messages) {
                return call(DictionaryTool.toolName, id: "mock-call-1", argument: "word", value: word)
            }
            guard let found = article(in: everything(in: messages)) else {
                return ChatMessage(role: "assistant", content: "Макет: слова «\(word)» в словаре нет.")
            }
            return ChatMessage(role: "assistant", content: "Макет: по словарю «\(found.lemma)» — \(found.firstSense).")
        }
        for verb in ["раньше", "before", "дальше", "after"] where lowered.hasPrefix(verb) {
            let before = verb == "раньше" || verb == "before"
            if !sawToolResult(in: messages) {
                return call(ContextTool.toolName, id: "mock-context-1", argument: "direction", value: before ? "before" : "after")
            }
            let place = before ? "перед этим местом" : "после этого места"
            let words = firstWordsRead(in: messages)
            return ChatMessage(
                role: "assistant",
                content: words.isEmpty ? "Макет: \(place) в главе ничего нет." : "Макет: \(place) в книге — «\(words)…»."
            )
        }
        return ChatMessage(
            role: "assistant",
            content: "Макет: на вопрос «\(question)» настоящая модель ответила бы по тексту книги."
        )
    }

    /// An X-ray: say how often the term has appeared, from the passages given.
    private static func xray(_ term: String, _ messages: [ChatMessage]) -> ChatMessage {
        let count = passageCount(in: everything(in: messages))
        // Nothing found: try once more in lower case, as a model would try a form.
        if count == 0, !sawToolResult(in: messages) {
            return call(SearchTool.toolName, id: "mock-search-1", argument: "query", value: term.lowercased())
        }
        if count == 0 {
            return ChatMessage(role: "assistant", content: "Макет: «\(term)» в прочитанной части книги не встречается.")
        }
        return ChatMessage(
            role: "assistant",
            content: "Макет: «\(term)» встречается в прочитанной части книги в \(count) отрывках; "
                + "настоящая модель объяснила бы по ним, кто или что это."
        )
    }

    /// What a search endpoint would answer: two pages about the query, in
    /// the shape `WebSearchTool` reads.
    static func webSearch(_ query: String) -> Any {
        let page = { (title: String, url: String, snippet: String) -> [String: Any] in
            ["title": title, "url": url, "snippet": snippet]
        }
        return ["results": [
            page("\(query) — Wikipédia", "https://fr.wikipedia.org/wiki/\(query)",
                 "Макет: страница энциклопедии о «\(query)». Настоящий поиск вернул бы начало статьи."),
            page("\(query) : définition", "https://example.org/definition/\(query)",
                 "Макет: словарная страница о «\(query)»."),
        ]]
    }

    private static func call(_ tool: String, id: String, argument: String, value: String) -> ChatMessage {
        let arguments = (try? JSONSerialization.data(withJSONObject: [argument: value]))
            .flatMap { String(data: $0, encoding: .utf8) } ?? "{}"
        return ChatMessage(
            role: "assistant",
            toolCalls: [ChatMessage.ToolCall(id: id, function: .init(name: tool, arguments: arguments))]
        )
    }

    // MARK: - Reading the conversation

    private struct Article {
        let lemma: String
        let senses: String

        var firstSense: String {
            senses.components(separatedBy: "; ").first ?? senses
        }
    }

    private struct Form {
        let lemma: String
        let grammar: String
    }

    private static func firstUserMessage(in messages: [ChatMessage]) -> String? {
        messages.first { $0.role == "user" }?.content
    }

    /// Pulls one labelled line out of the question the prompt built.
    private static func value(after label: String, in messages: [ChatMessage]) -> String? {
        firstUserMessage(in: messages)?
            .split(separator: "\n")
            .first { $0.hasPrefix(label) }
            .map { String($0.dropFirst(label.count)) }
    }

    private static func everything(in messages: [ChatMessage]) -> String {
        messages.compactMap(\.content).joined(separator: "\n")
    }

    private static func sawToolResult(in messages: [ChatMessage]) -> Bool {
        messages.contains { $0.role == "tool" }
    }

    /// The first words `expand_context` read, from the last tool answer: its
    /// first line names the direction, the text follows.
    private static func firstWordsRead(in messages: [ChatMessage]) -> String {
        let answer = messages.last { $0.role == "tool" }?.content ?? ""
        let lines = answer.split(separator: "\n", omittingEmptySubsequences: false)
        guard lines.count >= 2 else { return "" }
        return lines[1].split(separator: " ", omittingEmptySubsequences: false).prefix(6).joined(separator: " ")
    }

    /// Matches an article line from `DictionaryLookup.summary`.
    private static func article(in text: String) -> Article? {
        let pattern = /- (.+) \[(.*)\]: (.+)/
        guard let match = text.firstMatch(of: pattern) else { return nil }
        return Article(lemma: String(match.1), senses: String(match.3))
    }

    /// Matches a form line from `DictionaryLookup.summary`.
    private static func form(in text: String) -> Form? {
        let pattern = /- form of “(.+)” \((.+)\)/
        guard let match = text.firstMatch(of: pattern) else { return nil }
        return Form(lemma: String(match.1), grammar: String(match.2))
    }

    /// How many numbered passages `SearchTool.summary` listed in `text`.
    private static func passageCount(in text: String) -> Int {
        text.split(separator: "\n").filter { line in
            let digits = line.prefix { $0.isNumber }
            return !digits.isEmpty && line.dropFirst(digits.count).hasPrefix(". ")
        }.count
    }
}
