#include "DictionaryLookup.hpp"

#include "../../Support/Text.hpp"

#include <set>

std::string DictionaryLookup::summary() const {
    if (isEmpty() && forms.empty()) {
        return "No dictionary entry for “" + query + "”.";
    }

    std::vector<std::string> lines = {"Dictionary results for “" + query + "”:"};
    for (const auto& form : forms) {
        std::vector<std::string> grammar;
        for (const auto& part : {form.partOfSpeech, form.gender, form.number}) {
            if (!part.empty()) grammar.push_back(part);
        }
        for (const auto& feature : form.features) grammar.push_back(feature);
        lines.push_back("- form of “" + form.lemma + "” (" + Text::join(grammar, " ") + ")");
    }

    std::set<std::string> sources;
    for (const auto& article : articles) {
        if (!article.source.empty()) sources.insert(article.source);
    }
    bool manySources = sources.size() > 1;
    for (const auto& article : articles) {
        std::string partOfSpeech = article.partOfSpeech.empty() ? "" : " [" + article.partOfSpeech + "]";
        std::string source = manySources && !article.source.empty() ? " (" + article.source + ")" : "";
        lines.push_back("- " + article.lemma + partOfSpeech + source + ": " + Text::join(article.senses, "; "));
    }
    return Text::join(lines, "\n");
}
