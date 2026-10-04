// Explains a word by handing the dictionary's answer to the model and
// letting it ask for more entries — or search the book — until it can
// commit to a meaning.
import { system, user } from "../Domain/AI/ChatMessage.js";
import { explanationQuestion, explanationSystem, explanationTools } from "../Domain/AI/ExplanationPrompt.js";
import { decodeExplanation } from "../Domain/AI/WordExplanation.js";
import { lookupSummary } from "../Domain/Dictionary/DictionaryLookup.js";
import { converse } from "./ToolRunner.js";

export async function explainWord(word, sentence, settings, available) {
  const lookup = await available.dictionary.lookup(word, available.packs);
  const messages = [
    system(explanationSystem(settings.language)),
    user(explanationQuestion(word, sentence, lookupSummary(lookup))),
  ];
  const reply = await converse(settings, messages, explanationTools, true, available);
  const explanation = decodeExplanation(reply.content ?? "");
  if (!explanation) throw new Error("The model's answer could not be read.");
  return explanation;
}
