// The conversation sent to the model. What it is about — the page on
// screen, a word just explained, what the book says about a name — goes in
// once, with the first question. Turns are `{ isReader, text }`.
import { assistant, system, user } from "./ChatMessage.js";
import { contextTool, dictionaryTool, searchTool } from "./Tools.js";

/** `language` is the one every answer is written in. */
export function chatSystem(language) {
  return "Ты помогаешь читателю разобраться с книгой на иностранном языке. "
    + `Отвечай на языке «${language}», коротко и по делу. Можно объяснять грамматику, разбирать `
    + "предложения, пересказывать содержание и отвечать на вопросы о тексте.\n"
    + "\n"
    + "Инструмент lookup_dictionary ищет слово в офлайн-словаре читателя. Пользуйся им, когда "
    + "спрашивают о слове, которого нет в контексте, или просят сравнить слова, — и отвечай по "
    + "статье, а не по памяти; если статьи нет, попробуй другую форму, а потом скажи, что "
    + "отвечаешь без словаря.\n"
    + "\n"
    + "Инструмент search_book ищет слово или фразу в тексте книги — до места, до которого "
    + "читатель дочитал, — и в других книгах той же серии. Пользуйся им, когда вопрос о том, "
    + "что было раньше: о персонаже, месте, событии, о том, где слово уже встречалось. "
    + "Не пересказывай того, чего читатель ещё не читал.\n"
    + "\n"
    + "Инструмент expand_context возвращает текст книги прямо перед тем, о чём разговор, или сразу "
    + "после него. Пользуйся им, когда для ответа не хватает соседнего текста: начала сцены, "
    + "предыдущей реплики, конца фразы на следующей странице.";
}

export function pageContext(page) {
  return `Страница:\n${page}`;
}

export function wordContext(word, sentence, explanation) {
  let context = `Слово: ${word}\nПредложение: ${sentence}\nНачальная форма: ${explanation.lemma}`;
  if (explanation.formNote) context += `\nФорма: ${explanation.formNote}`;
  context += `\nОбъяснение: ${explanation.meaning}`;
  if (explanation.guessed) context += "\n(Объяснение — догадка, словарной статьи не было.)";
  return context;
}

export function xrayContext(term, answer) {
  return `Термин из книги: ${term}\nЧто о нём известно по книге: ${answer}`;
}

export function chatMessages(context, turns, language) {
  return [
    system(chatSystem(language)),
    ...turns.map((turn, index) => {
      if (!turn.isReader) return assistant(turn.text);
      return user(index === 0 ? `${context}\n\nВопрос: ${turn.text}` : turn.text);
    }),
  ];
}

/** The dictionary, the book search and the text around the passage, all open to the model in a conversation. */
export const chatTools = [dictionaryTool, searchTool, contextTool];
