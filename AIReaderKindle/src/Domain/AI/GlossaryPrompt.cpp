#include "GlossaryPrompt.hpp"

#include "../Dictionary/WordNormalizer.hpp"
#include "Support/Json.hpp"

#include <glib.h>

#include <cstdlib>

namespace GlossaryPrompt {

std::string system(const std::string& language) {
    return
    "Ты составляешь словарик к книге на иностранном языке для читателя, который её читает. "
    "Тебе дают пронумерованный список слов — в той форме, в какой они стоят в книге, — и под "
    "каждым один–три отрывка, где слово встречается.\n"
    "\n"
    "Для каждого слова напиши:\n"
    "- lemma — начальную (словарную) форму слова, на языке книги;\n"
    "- form_note — коротко, в какой форме слово стоит в тексте («мн. ч.», «прош. вр., 3 л. ед. ч.»); "
    "пусто, если это и есть начальная форма;\n"
    "- meaning — что слово значит в этих отрывках, коротко, как в карманном словаре. Если в отрывках "
    "оно в разных значениях, перечисли их через «; ». Если слово здесь — часть устойчивого выражения, "
    "назови выражение и что оно значит.\n"
    "\n"
    "Слово с апострофом (l'homme, qu'il) объясни целиком: какое слово сокращено и какое стоит после "
    "апострофа.\n"
    "\n"
    "Имя или название объясни только по отрывкам — кто или что это, — а не по тому, что ты знаешь "
    "о книге из других источников. Не рассказывай, что случится дальше.\n"
    "\n"
    "Ответ — только JSON-объект, без пояснений вокруг; form_note и meaning — на языке «" + language + "»:\n"
    "{\"words\": [{\"n\": номер слова в списке, \"lemma\": \"...\", \"form_note\": \"...\", \"meaning\": \"...\"}]}\n"
    "Объясни каждое слово из списка, ни одного не пропускай.";
}

std::string question(const std::vector<BookWord>& words) {
    std::string text;
    for (size_t i = 0; i < words.size(); ++i) {
        if (i > 0) text += "\n";
        text += std::to_string(i + 1) + ". " + words[i].spelling;
        for (const auto& example : words[i].examples) text += "\n   — " + example;
    }
    return text;
}

namespace {

/// Whitespace collapsed, and double quotes made single: word lists split on
/// tabs and drop double quotes.
std::string clean(const Json& value) {
    if (!value.isString()) return "";
    std::string text;
    bool space = false;
    const std::string& raw = value.string();
    for (const char* p = raw.c_str(); *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (g_unichar_isspace(c)) {
            space = true;
            continue;
        }
        if (space && !text.empty()) text += ' ';
        space = false;
        if (c == '"') text += '\'';
        else text.append(p, g_utf8_next_char(p) - p);
    }
    return text;
}

int numberOf(const Json& value) {
    if (value.isNumber()) return static_cast<int>(value.number());
    if (value.isString()) return std::atoi(value.string().c_str());
    return 0;
}

std::string definition(const std::string& form, const Json& item) {
    std::string meaning = clean(item.at("meaning"));
    if (meaning.empty()) return "";
    std::string lemma = clean(item.at("lemma"));
    if (lemma.empty() || WordNormalizer::normalize(lemma) == form) return meaning;
    std::string note = clean(item.at("form_note"));
    return note.empty() ? lemma + ": " + meaning : lemma + " (" + note + "): " + meaning;
}

}  // namespace

std::optional<std::map<std::string, std::string>> definitions(const std::string& content, const std::vector<BookWord>& words) {
    auto start = content.find('{');
    auto end = content.rfind('}');
    if (start == std::string::npos || end == std::string::npos || end <= start) return std::nullopt;
    auto json = Json::parse(content.substr(start, end - start + 1));
    if (!json || !json->isObject() || !json->at("words").isArray()) return std::nullopt;

    std::map<std::string, std::string> result;
    for (const auto& item : json->at("words").items()) {
        if (!item.isObject()) continue;
        int number = numberOf(item.at("n"));
        if (number < 1 || number > static_cast<int>(words.size())) continue;
        const std::string& form = words[number - 1].form;
        std::string text = definition(form, item);
        if (!text.empty()) result[form] = text;
    }
    return result;
}

}  // namespace GlossaryPrompt
