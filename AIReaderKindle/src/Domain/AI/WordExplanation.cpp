#include "WordExplanation.hpp"

Json WordExplanation::toJson() const {
    Json json = Json::object();
    json.set("lemma", lemma);
    json.set("form_note", formNote);
    json.set("meaning", meaning);
    json.set("guessed", guessed);
    json.set("confidence", confidence);
    return json;
}

std::optional<WordExplanation> WordExplanation::decode(const std::string& content) {
    auto start = content.find('{');
    auto end = content.rfind('}');
    if (start == std::string::npos || end == std::string::npos || end < start) return std::nullopt;
    auto json = Json::parse(content.substr(start, end - start + 1));
    if (!json || !json->isObject() || !json->has("meaning")) return std::nullopt;
    return WordExplanation{
        json->at("lemma").string(),
        json->at("form_note").string(),
        json->at("meaning").string(),
        json->at("guessed").boolean(),
        json->at("confidence").number(),
    };
}
