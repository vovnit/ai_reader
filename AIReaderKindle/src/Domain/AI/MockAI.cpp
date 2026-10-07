#include "MockAI.hpp"

#include "../../Support/Text.hpp"
#include "ContextTool.hpp"
#include "DictionaryTool.hpp"
#include "SearchTool.hpp"
#include "WebSearchTool.hpp"
#include "WordExplanation.hpp"

namespace MockAI {

const std::vector<std::string> models = {"mock-medium", "mock-small"};

namespace {

struct Article {
    std::string lemma;
    std::string senses;

    std::string firstSense() const {
        auto separator = senses.find("; ");
        return separator == std::string::npos ? senses : senses.substr(0, separator);
    }
};

struct Form {
    std::string lemma;
    std::string grammar;
};

const ChatMessage* firstUserMessage(const std::vector<ChatMessage>& messages) {
    for (const auto& message : messages) {
        if (message.role == "user" && message.content) return &message;
    }
    return nullptr;
}

/// Pulls one labelled line out of the question the prompt built.
std::optional<std::string> value(const std::string& label, const std::vector<ChatMessage>& messages) {
    const ChatMessage* first = firstUserMessage(messages);
    if (!first) return std::nullopt;
    for (const auto& line : Text::split(*first->content, '\n')) {
        if (Text::startsWith(line, label)) return line.substr(label.size());
    }
    return std::nullopt;
}

/// Matches an article line from `DictionaryLookup::summary`:
/// `- lemma [pos]: senses` or `- lemma: senses`.
std::optional<Article> article(const std::string& text) {
    for (const auto& line : Text::split(text, '\n')) {
        if (!Text::startsWith(line, "- ") || Text::startsWith(line, "- form of “")) continue;
        auto colon = line.find(": ");
        if (colon == std::string::npos) continue;
        std::string head = line.substr(2, colon - 2);
        auto bracket = head.find(" [");
        if (bracket != std::string::npos) head = head.substr(0, bracket);
        auto paren = head.find(" (");
        if (paren != std::string::npos) head = head.substr(0, paren);
        return Article{head, line.substr(colon + 2)};
    }
    return std::nullopt;
}

/// Matches a form line from `DictionaryLookup::summary`: `- form of “lemma” (grammar)`.
std::optional<Form> form(const std::string& text) {
    const std::string prefix = "- form of “";
    for (const auto& line : Text::split(text, '\n')) {
        if (!Text::startsWith(line, prefix)) continue;
        auto close = line.find("” (");
        if (close == std::string::npos || line.back() != ')') continue;
        std::string lemma = line.substr(prefix.size(), close - prefix.size());
        size_t grammarStart = close + std::string("” (").size();
        return Form{lemma, line.substr(grammarStart, line.size() - grammarStart - 1)};
    }
    return std::nullopt;
}

/// How many numbered passages `SearchTool::summary` listed in `text`.
int passageCount(const std::string& text) {
    int count = 0;
    for (const auto& line : Text::split(text, '\n')) {
        size_t digits = 0;
        while (digits < line.size() && line[digits] >= '0' && line[digits] <= '9') ++digits;
        if (digits > 0 && line.compare(digits, 2, ". ") == 0) ++count;
    }
    return count;
}

std::string everything(const std::vector<ChatMessage>& messages, bool* sawToolResult) {
    std::string text;
    for (const auto& message : messages) {
        if (message.content) text += *message.content + "\n";
        if (message.role == "tool") *sawToolResult = true;
    }
    return text;
}

ChatMessage searchCall(const std::string& query) {
    ChatMessage call;
    call.role = "assistant";
    Json arguments = Json::object();
    arguments.set("query", query);
    call.toolCalls.push_back({"mock-search-1", SearchTool::toolName, arguments.dump()});
    return call;
}

ChatMessage webCall(const std::string& query) {
    ChatMessage call;
    call.role = "assistant";
    Json arguments = Json::object();
    arguments.set("query", query);
    call.toolCalls.push_back({"mock-web-1", WebSearchTool::toolName, arguments.dump()});
    return call;
}

ChatMessage contextCall(const std::string& direction) {
    ChatMessage call;
    call.role = "assistant";
    Json arguments = Json::object();
    arguments.set("direction", direction);
    call.toolCalls.push_back({"mock-context-1", ContextTool::toolName, arguments.dump()});
    return call;
}

/// The first words `expand_context` read, from the last tool answer: its
/// first line names the direction, the text follows.
std::string firstWordsRead(const std::vector<ChatMessage>& messages) {
    std::string answer;
    for (const auto& message : messages) {
        if (message.role == "tool" && message.content) answer = *message.content;
    }
    auto lines = Text::split(answer, '\n');
    if (lines.size() < 2) return "";
    auto words = Text::split(lines[1], ' ');
    if (words.size() > 6) words.resize(6);
    return Text::join(words, " ");
}

ChatMessage lookupCall(const std::string& word) {
    ChatMessage call;
    call.role = "assistant";
    Json arguments = Json::object();
    arguments.set("word", word);
    call.toolCalls.push_back({"mock-call-1", DictionaryTool::toolName, arguments.dump()});
    return call;
}

/// A conversation: echo the question; asked to find something, search the
/// book for it, asked to look something up online, search the web, asked
/// what a word means, open the dictionary, and asked what came before or
/// comes after, read around the passage — the way a real model would.
ChatMessage chat(const std::vector<ChatMessage>& messages) {
    std::string question;
    for (const auto& message : messages) {
        if (message.role == "user" && message.content) question = *message.content;
    }
    auto marker = question.rfind("Вопрос: ");
    if (marker != std::string::npos) question = question.substr(marker + std::string("Вопрос: ").size());

    bool sawToolResult = false;
    std::string text = everything(messages, &sawToolResult);
    std::string lowered = Text::lower(question);
    for (const char* verb : {"найди ", "find "}) {
        if (!Text::startsWith(lowered, verb)) continue;
        std::string query = Text::trim(question.substr(std::string(verb).size()));
        if (!sawToolResult) return searchCall(query);
        return ChatMessage::assistant("Макет: по запросу «" + query + "» в книге нашлось "
                                      + std::to_string(passageCount(text)) + " отрывков.");
    }
    for (const char* verb : {"поищи ", "search "}) {
        if (!Text::startsWith(lowered, verb)) continue;
        std::string query = Text::trim(question.substr(std::string(verb).size()));
        if (!sawToolResult) return webCall(query);
        return ChatMessage::assistant("Макет: в интернете по запросу «" + query + "» нашлось "
                                      + std::to_string(passageCount(text)) + " страниц.");
    }
    for (const char* verb : {"что значит ", "what does "}) {
        if (!Text::startsWith(lowered, verb)) continue;
        std::string word = Text::trim(question.substr(std::string(verb).size()));
        while (!word.empty() && (word.back() == '?' || word.back() == '.')) word.pop_back();
        if (!sawToolResult) return lookupCall(word);
        auto found = article(text);
        return ChatMessage::assistant(found
            ? "Макет: по словарю «" + found->lemma + "» — " + found->firstSense() + "."
            : "Макет: слова «" + word + "» в словаре нет.");
    }
    for (const char* verb : {"раньше", "before", "дальше", "after"}) {
        if (!Text::startsWith(lowered, verb)) continue;
        bool before = std::string(verb) == "раньше" || std::string(verb) == "before";
        if (!sawToolResult) return contextCall(before ? "before" : "after");
        std::string place = before ? "перед этим местом" : "после этого места";
        std::string words = firstWordsRead(messages);
        return ChatMessage::assistant(words.empty()
            ? "Макет: " + place + " в главе ничего нет."
            : "Макет: " + place + " в книге — «" + words + "…».");
    }
    return ChatMessage::assistant("Макет: на вопрос «" + question + "» настоящая модель ответила бы по тексту книги.");
}

/// An X-ray: say how often the term has appeared, from the passages given.
ChatMessage xray(const std::string& term, const std::vector<ChatMessage>& messages) {
    bool sawToolResult = false;
    std::string text = everything(messages, &sawToolResult);
    int count = passageCount(text);
    // Nothing found: try once more in lower case, as a model would try a form.
    if (count == 0 && !sawToolResult) return searchCall(Text::lower(term));
    if (count == 0) return ChatMessage::assistant("Макет: «" + term + "» в прочитанной части книги не встречается.");
    return ChatMessage::assistant("Макет: «" + term + "» встречается в прочитанной части книги в "
                                  + std::to_string(count) + " отрывках; настоящая модель объяснила бы по ним, кто или что это.");
}

/// A word lookup: answer from the dictionary material in the conversation.
ChatMessage lookup(const std::string& word, const std::vector<ChatMessage>& messages) {
    bool sawToolResult = false;
    std::string text = everything(messages, &sawToolResult);
    auto found = article(text);
    auto reading = form(text);

    // With nothing to go on, ask the dictionary once more — the same move a
    // real model makes when the first entries are unusable.
    if (!found && !sawToolResult) return lookupCall(reading ? reading->lemma : word);

    WordExplanation explanation;
    explanation.lemma = found ? found->lemma : reading ? reading->lemma : word;
    explanation.formNote = reading
        ? "«" + word + "» — форма слова «" + reading->lemma + "» (" + reading->grammar + ")."
        : "«" + word + "» — начальная форма.";
    explanation.meaning = found ? found->firstSense() : "Значение в словаре не найдено (макет).";
    explanation.guessed = !found;
    explanation.confidence = found ? 0.9 : 0.35;
    return ChatMessage::assistant(explanation.toJson().dump());
}

/// A glossary batch: every numbered word defined from nothing but its spelling.
ChatMessage glossary(const std::vector<ChatMessage>& messages) {
    Json words = Json::array();
    const ChatMessage* first = firstUserMessage(messages);
    for (const auto& line : Text::split(first ? *first->content : "", '\n')) {
        size_t digits = 0;
        while (digits < line.size() && line[digits] >= '0' && line[digits] <= '9') ++digits;
        if (digits == 0 || line.compare(digits, 2, ". ") != 0 || line.size() == digits + 2) continue;
        std::string spelling = line.substr(digits + 2);
        Json word = Json::object();
        word.set("n", std::stoi(line.substr(0, digits)));
        word.set("lemma", Text::lower(spelling));
        word.set("form_note", "");
        word.set("meaning", "«" + spelling + "» в книге (макет)");
        words.push(word);
    }
    Json answer = Json::object();
    answer.set("words", words);
    return ChatMessage::assistant(answer.dump());
}

}  // namespace

ChatMessage reply(const std::vector<ChatMessage>& messages) {
    // Which prompt built this: a glossary's system prompt says so, the X-ray
    // names a term, a conversation ends its first message with a question, a
    // lookup names a word.
    for (const auto& message : messages) {
        if (message.role == "system" && message.content && Text::startsWith(*message.content, "Ты составляешь словарик")) {
            return glossary(messages);
        }
    }
    if (auto term = value("Термин: ", messages)) return xray(*term, messages);
    const ChatMessage* first = firstUserMessage(messages);
    if (first && Text::contains(*first->content, "\nВопрос: ")) return chat(messages);
    if (auto word = value("Слово: ", messages)) return lookup(*word, messages);
    return chat(messages);
}

Json webSearch(const std::string& query) {
    auto page = [&](const std::string& title, const std::string& url, const std::string& snippet) {
        Json item = Json::object();
        item.set("title", title);
        item.set("url", url);
        item.set("snippet", snippet);
        return item;
    };
    Json output = Json::object();
    output.set("results", Json(std::vector<Json>{
        page(query + " — Wikipédia", "https://fr.wikipedia.org/wiki/" + query,
             "Макет: страница энциклопедии о «" + query + "». Настоящий поиск вернул бы начало статьи."),
        page(query + " : définition", "https://example.org/definition/" + query,
             "Макет: словарная страница о «" + query + "»."),
    }));
    return output;
}

}  // namespace MockAI
