// What the model is asked to write a book's glossary — a batch of the book's
// word forms, each with the first places it appears — and how its answer
// becomes definitions. The wording is the other apps' and DictionaryTool's
// `glossary_prompt.py`, word for word.
import { normalizeWord } from "../Dictionary/WordNormalizer.js";

/** Forms asked about in one request. */
export const glossaryBatch = 50;
/** Room for a batch's definitions; a lookup's limit would cut the answer short. */
export const glossaryMaxTokens = 4000;
/** What a form costs, question and answer together, as measured on a short book. */
export const glossaryTokensPerWord = 120;

/** `language` is the one the definitions are written in. */
export function glossarySystem(language) {
  return "Ты составляешь словарик к книге на иностранном языке для читателя, который её читает. "
    + "Тебе дают пронумерованный список слов — в той форме, в какой они стоят в книге, — и под "
    + "каждым один–три отрывка, где слово встречается.\n"
    + "\n"
    + "Для каждого слова напиши:\n"
    + "- lemma — начальную (словарную) форму слова, на языке книги;\n"
    + "- form_note — коротко, в какой форме слово стоит в тексте («мн. ч.», «прош. вр., 3 л. ед. ч.»); "
    + "пусто, если это и есть начальная форма;\n"
    + "- meaning — что слово значит в этих отрывках, коротко, как в карманном словаре. Если в отрывках "
    + "оно в разных значениях, перечисли их через «; ». Если слово здесь — часть устойчивого выражения, "
    + "назови выражение и что оно значит.\n"
    + "\n"
    + "Слово с апострофом (l'homme, qu'il) объясни целиком: какое слово сокращено и какое стоит после "
    + "апострофа.\n"
    + "\n"
    + "Имя или название объясни только по отрывкам — кто или что это, — а не по тому, что ты знаешь "
    + "о книге из других источников. Не рассказывай, что случится дальше.\n"
    + "\n"
    + `Ответ — только JSON-объект, без пояснений вокруг; form_note и meaning — на языке «${language}»:\n`
    + "{\"words\": [{\"n\": номер слова в списке, \"lemma\": \"...\", \"form_note\": \"...\", \"meaning\": \"...\"}]}\n"
    + "Объясни каждое слово из списка, ни одного не пропускай.";
}

/** `words` are `bookWords`' `{ form, spelling, examples }`. */
export function glossaryQuestion(words) {
  return words.flatMap((word, index) => [`${index + 1}. ${word.spelling}`, ...word.examples.map((example) => `   — ${example}`)]).join("\n");
}

/** Form → definition for each word the answer covers, or null when the answer cannot be read. A word the model skipped is simply absent. */
export function glossaryDefinitions(content, words) {
  const start = content.indexOf("{");
  const end = content.lastIndexOf("}");
  let items = null;
  try {
    if (start >= 0 && end > start) items = JSON.parse(content.slice(start, end + 1)).words;
  } catch {}
  if (!Array.isArray(items)) return null;

  const definitions = new Map();
  for (const item of items) {
    if (!item || typeof item !== "object") continue;
    const number = Number.parseInt(item.n, 10);
    if (!(number >= 1 && number <= words.length)) continue;
    const form = words[number - 1].form;
    const text = definition(form, item);
    if (text) definitions.set(form, text);
  }
  return definitions;
}

function definition(form, item) {
  const meaning = clean(item.meaning);
  if (!meaning) return "";
  const lemma = clean(item.lemma);
  if (!lemma || normalizeWord(lemma) === form) return meaning;
  const note = clean(item.form_note);
  return note ? `${lemma} (${note}): ${meaning}` : `${lemma}: ${meaning}`;
}

/** Whitespace collapsed, and double quotes made single: word lists split on tabs and drop double quotes. */
function clean(value) {
  return typeof value === "string" ? value.split(/\s+/).filter(Boolean).join(" ").replaceAll('"', "'") : "";
}
