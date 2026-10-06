// The tools the model may call, each a function with one required string
// argument, as the chat completions API describes them.

export const dictionaryToolName = "lookup_dictionary";
export const searchToolName = "search_book";
export const contextToolName = "expand_context";
export const webSearchToolName = "search_web";

/** `choices`, when given, are the only values the argument may take. */
function tool(name, description, argument, argumentDescription, choices = []) {
  const property = { type: "string", description: argumentDescription };
  if (choices.length) property.enum = choices;
  return {
    type: "function",
    function: {
      name,
      description,
      parameters: {
        type: "object",
        properties: { [argument]: property },
        required: [argument],
      },
    },
  };
}

/** The string argument out of a call's arguments, or `fallback` when the model sent something else. */
export function argument(args, name, fallback = "") {
  try {
    const value = JSON.parse(args)?.[name];
    return typeof value === "string" ? value : fallback;
  } catch {
    return fallback;
  }
}

/** Finds a word's articles in the offline packs. */
export const dictionaryTool = tool(
  dictionaryToolName,
  "Ищет слово в офлайн-словаре и возвращает найденные статьи.",
  "word",
  "Форма слова, которую нужно найти.",
);

/** Finds a phrase in the book being read, and in the other books of its group. */
export const searchTool = tool(
  searchToolName,
  "Ищет слово или фразу в тексте книги, которую читает пользователь (и в других книгах той же серии), "
    + "и возвращает отрывки, где она встречается, — только до места, до которого читатель дочитал.",
  "query",
  "Слово или короткая фраза, которую нужно найти в книге.",
);

/** Reads the book just before the passage a conversation is about, or just after it. */
export const contextTool = tool(
  contextToolName,
  "Возвращает текст книги, который идёт прямо перед тем местом, о котором речь (предложением "
    + "или страницей), или сразу после него. Каждый следующий вызов в ту же сторону читает дальше. "
    + "Используй, когда для понимания не хватает соседнего текста: к кому относится местоимение, "
    + "кто говорит, о чём шла речь абзацем выше.",
  "direction",
  "\"before\" — текст перед этим местом, \"after\" — текст после него.",
  ["before", "after"],
);

/** Finds pages about a name, a place, an event or an expression. */
export const webSearchTool = tool(
  webSearchToolName,
  "Ищет в интернете и возвращает несколько найденных страниц с фрагментами их текста. "
    + "Используй, когда ни словарь, ни книга не помогают: имя, место, событие, реалия, "
    + "название, сленг или выражение, которых нет в словаре. Не ищи то, что есть в словаре.",
  "query",
  "Что искать: короткий запрос, как в поисковой строке.",
);
