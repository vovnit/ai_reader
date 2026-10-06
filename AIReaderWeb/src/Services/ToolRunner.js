// Carries a conversation with the model, answering the tools it calls —
// the dictionary, the book search, the text around the passage and the
// web — until it answers in words. `available` is what the model may reach
// for: `{ scope: { corpus, upTo, passage }, packs, web, dictionary }`, the
// passage `{ bookId, chapter, start, end }`. A missing corpus or
// empty pack list answers that tool with nothing; the web is offered only
// once it is set up, since a search may be paid for, and always under the
// mock, which answers it itself.
import { toolResult } from "../Domain/AI/ChatMessage.js";
import { contextDirection, nothingAround, readAround, unknownDirection } from "../Domain/AI/ContextReading.js";
import { lookupSummary } from "../Domain/Dictionary/DictionaryLookup.js";
import { mockWebSearch } from "../Domain/AI/MockAI.js";
import { passageLimit, passagesSummary, webSummary } from "../Domain/AI/SearchSummary.js";
import { argument, contextToolName, dictionaryToolName, searchToolName, webSearchTool, webSearchToolName } from "../Domain/AI/Tools.js";
import { chat } from "./ChatApi.js";
import { usesMock, webConfigured } from "./Settings.js";
import { searchWeb } from "./WebSearch.js";

/** How many rounds of tool calls the model gets before it must answer. */
export const budget = 3;

/** `window` is how much of the book around the passage the model has read so far, widened by each `expand_context` call. */
async function answer(call, settings, available, window) {
  if (call.name === dictionaryToolName) {
    return lookupSummary(await available.dictionary.lookup(argument(call.arguments, "word"), available.packs ?? []));
  }
  if (call.name === searchToolName) {
    const query = argument(call.arguments, "query");
    const { corpus, upTo } = available.scope ?? {};
    if (!corpus || !query) return passagesSummary(query, [], false);
    return passagesSummary(query, await corpus.search(query, passageLimit, upTo), corpus.severalBooks);
  }
  if (call.name === contextToolName) {
    const direction = contextDirection(call.arguments);
    if (!direction) return unknownDirection;
    const corpus = available.scope?.corpus;
    if (!window || !corpus) return nothingAround;
    const chapter = await corpus.chapterText(window.bookId, window.chapter);
    return chapter ? readAround(direction, chapter, window) : nothingAround;
  }
  if (call.name === webSearchToolName) {
    const query = argument(call.arguments, "query");
    if (!query) return webSummary(query, null);
    // A failed search is told to the model, which can still answer from
    // what it has, rather than failing the whole conversation.
    try {
      // Pages in the book's language: a French name wants the French page.
      const language = available.scope?.corpus?.books[0]?.language ?? "";
      const output = usesMock(settings) ? mockWebSearch(query) : await searchWeb(available.web, query, language);
      return webSummary(query, output);
    } catch (error) {
      return `The web search failed: ${error.message}`;
    }
  }
  return `There is no tool called “${call.name}”.`;
}

/** The model's final message; `messages` grows by the calls and their answers. */
export async function converse(settings, messages, tools, jsonMode, available) {
  const offered = usesMock(settings) || (available.web && webConfigured(available.web)) ? [...tools, webSearchTool] : tools;
  const passage = available.scope?.passage;
  const window = passage ? { ...passage } : null;
  for (let round = 0; round <= budget; round++) {
    const reply = await chat(settings, messages, offered, jsonMode);
    if (!reply.toolCalls.length) return reply;
    messages.push(reply);
    for (const call of reply.toolCalls) messages.push(toolResult(await answer(call, settings, available, window), call.id));
  }
  throw new Error("The model kept searching without answering.");
}
