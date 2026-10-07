import Foundation

/// What the model is asked to write a book's glossary — a batch of the book's
/// word forms, each with the first places it appears — and how its answer
/// becomes definitions. The wording is the other apps' and DictionaryTool's
/// `glossary_prompt.py`, word for word.
enum GlossaryPrompt {
    /// Forms asked about in one request.
    static let batchSize = 50
    /// Room for a batch's definitions; a lookup's limit would cut the answer short.
    static let maxTokens = 4000
    /// What a form costs, question and answer together, as measured on a short book.
    static let tokensPerWord = 120

    /// `language` is the one the definitions are written in.
    static func system(language: String) -> String {
        """
        Ты составляешь словарик к книге на иностранном языке для читателя, который её читает. \
        Тебе дают пронумерованный список слов — в той форме, в какой они стоят в книге, — и под \
        каждым один–три отрывка, где слово встречается.

        Для каждого слова напиши:
        - lemma — начальную (словарную) форму слова, на языке книги;
        - form_note — коротко, в какой форме слово стоит в тексте («мн. ч.», «прош. вр., 3 л. ед. ч.»); \
        пусто, если это и есть начальная форма;
        - meaning — что слово значит в этих отрывках, коротко, как в карманном словаре. Если в отрывках \
        оно в разных значениях, перечисли их через «; ». Если слово здесь — часть устойчивого выражения, \
        назови выражение и что оно значит.

        Слово с апострофом (l'homme, qu'il) объясни целиком: какое слово сокращено и какое стоит после \
        апострофа.

        Имя или название объясни только по отрывкам — кто или что это, — а не по тому, что ты знаешь \
        о книге из других источников. Не рассказывай, что случится дальше.

        Ответ — только JSON-объект, без пояснений вокруг; form_note и meaning — на языке «\(language)»:
        {"words": [{"n": номер слова в списке, "lemma": "...", "form_note": "...", "meaning": "..."}]}
        Объясни каждое слово из списка, ни одного не пропускай.
        """
    }

    static func question(_ words: [BookWord]) -> String {
        words.enumerated().flatMap { index, word in
            ["\(index + 1). \(word.spelling)"] + word.examples.map { "   — \($0)" }
        }
        .joined(separator: "\n")
    }

    /// Form → definition for each word the answer covers, or nil when the
    /// answer cannot be read. A word the model skipped is simply absent.
    static func definitions(in content: String, for words: [BookWord]) -> [String: String]? {
        guard let start = content.firstIndex(of: "{"),
              let end = content.lastIndex(of: "}"),
              start < end,
              let json = try? JSONSerialization.jsonObject(with: Data(content[start...end].utf8)),
              let items = (json as? [String: Any])?["words"] as? [Any]
        else { return nil }

        var definitions: [String: String] = [:]
        for case let item as [String: Any] in items {
            guard let number = self.number(item["n"]), words.indices.contains(number - 1) else { continue }
            let form = words[number - 1].form
            let text = definition(of: form, from: item)
            if !text.isEmpty { definitions[form] = text }
        }
        return definitions
    }

    private static func number(_ value: Any?) -> Int? {
        if let number = value as? Int { return number }
        if let number = value as? Double { return Int(number) }
        return (value as? String).flatMap { Int($0.prefix { $0.isNumber }) }
    }

    private static func definition(of form: String, from item: [String: Any]) -> String {
        let meaning = clean(item["meaning"])
        guard !meaning.isEmpty else { return "" }
        let lemma = clean(item["lemma"])
        guard !lemma.isEmpty, WordNormalizer.normalize(lemma) != form else { return meaning }
        let note = clean(item["form_note"])
        return note.isEmpty ? "\(lemma): \(meaning)" : "\(lemma) (\(note)): \(meaning)"
    }

    /// Whitespace collapsed, and double quotes made single: word lists split
    /// on tabs and drop double quotes.
    private static func clean(_ value: Any?) -> String {
        guard let text = value as? String else { return "" }
        return text.split(whereSeparator: \.isWhitespace).joined(separator: " ")
            .replacingOccurrences(of: "\"", with: "'")
    }
}
