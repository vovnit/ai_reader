#pragma once

#include <string>
#include <vector>

/// Everything the offline dictionary knows about one written form.
struct DictionaryLookup {
    /// A grammatical reading of the form that was tapped.
    struct Form {
        std::string lemma;
        std::string partOfSpeech;
        std::string gender;
        std::string number;
        std::vector<std::string> features;
    };

    /// One dictionary article, belonging to a lemma.
    struct Article {
        std::string lemma;
        std::string partOfSpeech;
        std::vector<std::string> senses;
        /// The dictionary it came from, worth naming when more than one answered.
        std::string source;
    };

    std::string query;
    std::vector<Form> forms;
    std::vector<Article> articles;

    bool isEmpty() const { return articles.empty(); }

    /// A compact plain-text rendering, small enough to put in a prompt.
    std::string summary() const;
};
