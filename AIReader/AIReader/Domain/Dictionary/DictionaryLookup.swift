import Foundation

/// Everything the offline dictionary knows about one written form.
struct DictionaryLookup: Equatable, Sendable, Codable {
    /// A grammatical reading of the form that was typed or tapped.
    struct Form: Equatable, Sendable, Codable {
        var lemma: String
        var partOfSpeech: String
        var gender: String?
        var number: String?
        var features: [String]
    }

    /// One dictionary article, belonging to a lemma.
    struct Article: Equatable, Sendable, Codable {
        var lemma: String
        var partOfSpeech: String?
        var senses: [String]
        /// The dictionary it came from, worth naming when more than one answered.
        var source: String?
    }

    var query: String
    var forms: [Form] = []
    var articles: [Article] = []

    var isEmpty: Bool { articles.isEmpty }

    /// A compact plain-text rendering, small enough to put in a prompt.
    var summary: String {
        guard !isEmpty || !forms.isEmpty else {
            return "No dictionary entry for “\(query)”."
        }

        var lines = ["Dictionary results for “\(query)”:"]
        for form in forms {
            let grammar = ([form.partOfSpeech, form.gender, form.number] + form.features)
                .compactMap { $0 }
                .joined(separator: " ")
            lines.append("- form of “\(form.lemma)” (\(grammar))")
        }
        let manySources = Set(articles.compactMap(\.source)).count > 1
        for article in articles {
            let partOfSpeech = article.partOfSpeech.map { " [\($0)]" } ?? ""
            let source = manySources ? article.source.map { " (\($0))" } ?? "" : ""
            lines.append(
                "- \(article.lemma)\(partOfSpeech)\(source): \(article.senses.joined(separator: "; "))"
            )
        }
        return lines.joined(separator: "\n")
    }
}
