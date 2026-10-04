// Stands in for the model when the endpoint is `mock://ai`. It reads the
// same conversation a real model would and answers from the dictionary
// material in it, so a mocked run still exercises the prompt, the
// tool-calling loop and the JSON parsing. The same as the other apps'
// `MockAI`, reply for reply.
import { assistant, toolCall } from "./ChatMessage.js";
import { dictionaryToolName, searchToolName, webSearchToolName } from "./Tools.js";
import { explanationJson } from "./WordExplanation.js";

export const mockModels = ["mock-medium", "mock-small"];

function firstUserMessage(messages) {
  return messages.find((message) => message.role === "user" && message.content != null) ?? null;
}

/** One labelled line out of the question the prompt built. */
function value(label, messages) {
  const first = firstUserMessage(messages);
  if (!first) return null;
  const line = first.content.split("\n").find((candidate) => candidate.startsWith(label));
  return line === undefined ? null : line.slice(label.length);
}

/** An article line from `lookupSummary`: `- lemma [pos]: senses` or `- lemma: senses`. */
function article(text) {
  for (const line of text.split("\n")) {
    if (!line.startsWith("- ") || line.startsWith("- form of “")) continue;
    const colon = line.indexOf(": ");
    if (colon < 0) continue;
    let head = line.slice(2, colon);
    for (const opener of [" [", " ("]) if (head.includes(opener)) head = head.slice(0, head.indexOf(opener));
    const senses = line.slice(colon + 2);
    const separator = senses.indexOf("; ");
    return { lemma: head, firstSense: separator < 0 ? senses : senses.slice(0, separator) };
  }
  return null;
}

/** A form line from `lookupSummary`: `- form of “lemma” (grammar)`. */
function form(text) {
  const prefix = "- form of “";
  for (const line of text.split("\n")) {
    if (!line.startsWith(prefix)) continue;
    const close = line.indexOf("” (");
    if (close < 0 || !line.endsWith(")")) continue;
    return { lemma: line.slice(prefix.length, close), grammar: line.slice(close + 3, -1) };
  }
  return null;
}

/** How many numbered passages a summary listed. */
function passageCount(text) {
  return text.split("\n").filter((line) => /^\d+\. /.test(line)).length;
}

function everything(messages) {
  return {
    text: messages.filter((message) => message.content != null).map((message) => `${message.content}\n`).join(""),
    sawToolResult: messages.some((message) => message.role === "tool"),
  };
}

/**
 * A conversation: echo the question; asked to find something, search the
 * book; asked to look something up online, search the web; asked what a
 * word means, open the dictionary — the way a real model would.
 */
function chat(messages) {
  let question = messages.filter((message) => message.role === "user" && message.content != null).at(-1)?.content ?? "";
  const marker = question.lastIndexOf("Вопрос: ");
  if (marker >= 0) question = question.slice(marker + "Вопрос: ".length);
  const { text, sawToolResult } = everything(messages);
  const lowered = question.toLowerCase();
  for (const verb of ["найди ", "find "]) {
    if (!lowered.startsWith(verb)) continue;
    const query = question.slice(verb.length).trim();
    if (!sawToolResult) return toolCall("mock-search-1", searchToolName, "query", query);
    return assistant(`Макет: по запросу «${query}» в книге нашлось ${passageCount(text)} отрывков.`);
  }
  for (const verb of ["поищи ", "search "]) {
    if (!lowered.startsWith(verb)) continue;
    const query = question.slice(verb.length).trim();
    if (!sawToolResult) return toolCall("mock-web-1", webSearchToolName, "query", query);
    return assistant(`Макет: в интернете по запросу «${query}» нашлось ${passageCount(text)} страниц.`);
  }
  for (const verb of ["что значит ", "what does "]) {
    if (!lowered.startsWith(verb)) continue;
    const word = question.slice(verb.length).trim().replace(/[?.]+$/, "");
    if (!sawToolResult) return toolCall("mock-call-1", dictionaryToolName, "word", word);
    const found = article(text);
    return assistant(found
      ? `Макет: по словарю «${found.lemma}» — ${found.firstSense}.`
      : `Макет: слова «${word}» в словаре нет.`);
  }
  return assistant(`Макет: на вопрос «${question}» настоящая модель ответила бы по тексту книги.`);
}

/** An X-ray: say how often the term has appeared, from the passages given. */
function xray(term, messages) {
  const { text, sawToolResult } = everything(messages);
  const count = passageCount(text);
  // Nothing found: try once more in lower case, as a model would try a form.
  if (count === 0 && !sawToolResult) return toolCall("mock-search-1", searchToolName, "query", term.toLowerCase());
  if (count === 0) return assistant(`Макет: «${term}» в прочитанной части книги не встречается.`);
  return assistant(`Макет: «${term}» встречается в прочитанной части книги в ${count} отрывках; `
    + "настоящая модель объяснила бы по ним, кто или что это.");
}

/** A word lookup: answer from the dictionary material in the conversation. */
function lookup(word, messages) {
  const { text, sawToolResult } = everything(messages);
  const found = article(text);
  const reading = form(text);
  // With nothing to go on, ask the dictionary once more — the same move a
  // real model makes when the first entries are unusable.
  if (!found && !sawToolResult) return toolCall("mock-call-1", dictionaryToolName, "word", reading ? reading.lemma : word);
  return assistant(explanationJson({
    lemma: found ? found.lemma : reading ? reading.lemma : word,
    formNote: reading ? `«${word}» — форма слова «${reading.lemma}» (${reading.grammar}).` : `«${word}» — начальная форма.`,
    meaning: found ? found.firstSense : "Значение в словаре не найдено (макет).",
    guessed: !found,
    confidence: found ? 0.9 : 0.35,
  }));
}

export function mockReply(messages) {
  // Which prompt built this: the X-ray names a term, a conversation ends
  // its first message with a question, a lookup names a word.
  const term = value("Термин: ", messages);
  if (term !== null) return xray(term, messages);
  const first = firstUserMessage(messages);
  if (first && first.content.includes("\nВопрос: ")) return chat(messages);
  const word = value("Слово: ", messages);
  if (word !== null) return lookup(word, messages);
  return chat(messages);
}

/** What a web search endpoint would answer, in the shape TinyFish uses. */
export function mockWebSearch(query) {
  return {
    results: [
      {
        title: `${query} — Wikipédia`,
        url: `https://fr.wikipedia.org/wiki/${query}`,
        snippet: `Макет: страница энциклопедии о «${query}». Настоящий поиск вернул бы начало статьи.`,
      },
      { title: `${query} : définition`, url: `https://example.org/definition/${query}`, snippet: `Макет: словарная страница о «${query}».` },
    ],
  };
}
