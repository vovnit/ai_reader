// One batch of a book's glossary: the model asked what each word means
// where it first appears, and its answer read back as definitions.
import { system, user } from "../Domain/AI/ChatMessage.js";
import { glossaryDefinitions, glossaryMaxTokens, glossaryQuestion, glossarySystem } from "../Domain/AI/GlossaryPrompt.js";
import { chat } from "./ChatApi.js";

/** Form → definition for the `bookWords` given; a word the model skipped is absent. */
export async function defineWords(settings, words) {
  const messages = [system(glossarySystem(settings.language)), user(glossaryQuestion(words))];
  const reply = await chat(settings, messages, [], true, glossaryMaxTokens);
  const definitions = glossaryDefinitions(reply.content ?? "", words);
  if (!definitions) throw new Error("The model's answer could not be read.");
  return definitions;
}
