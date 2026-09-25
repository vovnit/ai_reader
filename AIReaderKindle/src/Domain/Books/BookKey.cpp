#include "BookKey.hpp"

#include "../../Support/Text.hpp"

#include <vector>

namespace BookKey {

namespace {

std::string normalize(const std::string& value) {
    std::vector<std::string> words;
    for (const auto& part : Text::split(Text::lower(value), ' ')) {
        for (const auto& word : Text::split(Text::replaceAll(Text::replaceAll(part, "\t", " "), "\n", " "), ' ')) {
            if (!word.empty()) words.push_back(word);
        }
    }
    return Text::join(words, " ");
}

}  // namespace

std::string make(const std::string& title, const std::string& author) {
    return normalize(title) + "|" + normalize(author);
}

}  // namespace BookKey
