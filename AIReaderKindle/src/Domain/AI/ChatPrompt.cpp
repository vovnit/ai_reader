#include "ChatPrompt.hpp"

#include "DictionaryTool.hpp"
#include "SearchTool.hpp"

namespace ChatPrompt {

const char* const system =
    "Ты помогаешь читателю разобраться с книгой на иностранном языке. "
    "Отвечай по-русски, коротко и по делу. Можно объяснять грамматику, разбирать "
    "предложения, пересказывать содержание и отвечать на вопросы о тексте.\n"
    "\n"
    "Инструмент lookup_dictionary ищет слово в офлайн-словаре читателя. Пользуйся им, когда "
    "спрашивают о слове, которого нет в контексте, или просят сравнить слова, — и отвечай по "
    "статье, а не по памяти; если статьи нет, попробуй другую форму, а потом скажи, что "
    "отвечаешь без словаря.\n"
    "\n"
    "Инструмент search_book ищет слово или фразу в тексте книги — до места, до которого "
    "читатель дочитал, — и в других книгах той же серии. Пользуйся им, когда вопрос о том, "
    "что было раньше: о персонаже, месте, событии, о том, где слово уже встречалось. "
    "Не пересказывай того, чего читатель ещё не читал.";

std::string pageContext(const std::string& page) {
    return "Страница:\n" + page;
}

std::string wordContext(const std::string& word, const std::string& sentence, const WordExplanation& explanation) {
    std::string context = "Слово: " + word + "\nПредложение: " + sentence
        + "\nНачальная форма: " + explanation.lemma;
    if (!explanation.formNote.empty()) context += "\nФорма: " + explanation.formNote;
    context += "\nОбъяснение: " + explanation.meaning;
    if (explanation.guessed) context += "\n(Объяснение — догадка, словарной статьи не было.)";
    return context;
}

std::string xrayContext(const std::string& term, const std::string& answer) {
    return "Термин из книги: " + term + "\nЧто о нём известно по книге: " + answer;
}

std::vector<ChatMessage> messages(const std::string& context, const std::vector<ChatTurn>& turns) {
    std::vector<ChatMessage> result = {ChatMessage::system(system)};
    // The context goes in once, attached to the first question.
    for (size_t index = 0; index < turns.size(); ++index) {
        const ChatTurn& turn = turns[index];
        if (!turn.isReader) {
            result.push_back(ChatMessage::assistant(turn.text));
            continue;
        }
        result.push_back(ChatMessage::user(index == 0 ? context + "\n\nВопрос: " + turn.text : turn.text));
    }
    return result;
}

Json tools() {
    return Json(std::vector<Json>{DictionaryTool::tool(), SearchTool::tool()});
}

}  // namespace ChatPrompt
