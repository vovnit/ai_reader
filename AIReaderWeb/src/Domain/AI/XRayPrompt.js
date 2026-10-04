// The conversation that asks what a name or a term means *in this book*:
// the passages where it has appeared so far are the only source.
import { system, user } from "./ChatMessage.js";
import { passagesSummary } from "./SearchSummary.js";

/** `language` is the one the answer is written in. */
export function xraySystem(language) {
  return "Ты помогаешь читателю книги на иностранном языке. Тебе дают имя, название или слово "
    + "и отрывки из книги, где оно встречается — только до того места, до которого читатель "
    + "дочитал.\n"
    + "\n"
    + "Объясни, кто или что это в этой книге: по самим отрывкам, а не по словарю и не по тому, "
    + "что ты знаешь о книге из других источников. Персонаж — кто он, как связан с другими; "
    + "место — что там происходит; предмет или авторское слово — что оно значит здесь. "
    + "Не рассказывай, что случится дальше.\n"
    + "\n"
    + "Если отрывков мало или они не дают ответа, вызови инструмент search_book с другой формой: "
    + "другим падежом, фамилией вместо имени, без артикля. Инструмент можно вызывать несколько раз.\n"
    + "\n"
    + `Отвечай на языке «${language}», коротко: два–пять предложений, без вступления. Если по книге ничего `
    + "понять нельзя, так и скажи.";
}

export function xrayMessages(term, hits, severalBooks, language) {
  return [system(xraySystem(language)), user(`Термин: ${term}\n\n${passagesSummary(term, hits, severalBooks)}`)];
}
