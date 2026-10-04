// The instructions and tools that drive a word explanation. The wording is
// the other apps', word for word, so a model answers the same everywhere.
import { dictionaryTool, searchTool } from "./Tools.js";

/** `language` is the one the explanation is written in. */
export function explanationSystem(language) {
  return "Ты помогаешь читателю понимать слова в книге на иностранном языке. "
    + "Тебе дают слово, предложение, в котором оно встретилось, и статьи из офлайн-словаря.\n"
    + "\n"
    + "Предложение дано только как контекст: по нему видно, в каком значении и в какой "
    + "форме стоит слово. Не переводи предложение — объясни само слово, коротко и "
    + `понятно, на языке «${language}».\n`
    + "\n"
    + "Если словарных статей нет или они не подходят, вызови инструмент lookup_dictionary с другой "
    + "формой слова: с предполагаемой начальной формой, без артикля или частицы, с другой "
    + "частью составного выражения. Инструмент можно вызывать несколько раз.\n"
    + "\n"
    + "Если слово похоже на имя, название или авторское слово, которого в словаре нет, вызови "
    + "инструмент search_book: он находит отрывки книги, где слово уже встречалось, и по ним "
    + "видно, кто или что это в этой книге.\n"
    + "\n"
    + "Если значение так и не нашлось, догадайся сам по контексту и честно отметь это.\n"
    + "\n"
    + `Ответ — только JSON-объект, без пояснений вокруг; form_note и meaning — на языке «${language}»:\n`
    + "{\n"
    + "  \"lemma\": \"начальная (словарная) форма слова\",\n"
    + "  \"form_note\": \"в какой форме слово стоит в тексте и как она связана с начальной формой\",\n"
    + "  \"meaning\": \"короткое понятное объяснение значения в этом предложении\",\n"
    + "  \"guessed\": true если ответ целиком основан на догадке, а не на словаре,\n"
    + "  \"confidence\": число от 0 до 1 — насколько ты уверен в значении\n"
    + "}";
}

/** `summary` is the dictionary's answer for the word, as `lookupSummary` writes it. */
export function explanationQuestion(word, sentence, summary) {
  return `Слово: ${word}\nПредложение: ${sentence}\n\n${summary}`;
}

export const explanationTools = [dictionaryTool, searchTool];
