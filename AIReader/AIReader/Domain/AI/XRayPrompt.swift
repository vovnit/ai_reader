import Foundation

/// The conversation that asks what a name or a term means *in this book*:
/// the passages where it has appeared so far are the only source.
enum XRayPrompt {
    /// The passages gathered before asking; at most this many go in.
    static let passageLimit = 12

    static let system = """
        Ты помогаешь читателю книги на иностранном языке. Тебе дают имя, название или слово \
        и отрывки из книги, где оно встречается — только до того места, до которого читатель \
        дочитал.

        Объясни, кто или что это в этой книге: по самим отрывкам, а не по словарю и не по тому, \
        что ты знаешь о книге из других источников. Персонаж — кто он, как связан с другими; \
        место — что там происходит; предмет или авторское слово — что оно значит здесь. \
        Не рассказывай, что случится дальше.

        Если отрывков мало или они не дают ответа, вызови инструмент search_book с другой формой: \
        другим падежом, фамилией вместо имени, без артикля. Инструмент можно вызывать несколько раз.

        Отвечай по-русски, коротко: два–пять предложений, без вступления. Если по книге ничего \
        понять нельзя, так и скажи.
        """

    static func messages(term: String, hits: [SearchHit], severalBooks: Bool) -> [ChatMessage] {
        [
            .system(system),
            .user("Термин: \(term)\n\n" + SearchTool.summary(query: term, hits: hits, severalBooks: severalBooks))
        ]
    }
}
