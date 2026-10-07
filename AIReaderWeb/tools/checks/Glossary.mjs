// A book's offline glossary: the words it is written from, the prompt and
// the reading of its answers — the Kindle check's cases, with the same
// expectations — and a run against the mock that fills, stops and resumes.
import { system, user } from "../../src/Domain/AI/ChatMessage.js";
import { glossaryDefinitions, glossaryQuestion, glossarySystem } from "../../src/Domain/AI/GlossaryPrompt.js";
import { mockReply } from "../../src/Domain/AI/MockAI.js";
import { bookWords } from "../../src/Domain/Reading/BookWords.js";
import { GlossaryFeature } from "../../src/Features/Glossary/GlossaryFeature.js";
import { mockEndpoint } from "../../src/Services/Settings.js";
import { check } from "./Check.mjs";
import { freshEnv } from "./Env.mjs";

export async function checkGlossary() {
  const forms = bookWords(["L’homme dit : c'est-à-dire 42 fois, l'homme !"], "fr").map((word) => word.form);
  check("glossary forms follow the word boundaries", forms.join(" ") === "l'homme dit c'est à dire fois", forms.join(" "));
  const chat = bookWords(["Le chat un.\nLe chat deux.", "Le chat trois.\nLe chat quatre."], "fr")[1];
  check("a form keeps its first three places", chat.examples.join(" | ") === "Le chat un. | Le chat deux. | Le chat trois.", chat.examples.join(" | "));
  const long = bookWords([`${"mot ".repeat(10)}chat${" mot".repeat(10)}.`], "fr")[1];
  check("an example is the words around it", long.examples[0] === `…${"mot ".repeat(8)}chat${" mot".repeat(8)}…`, long.examples[0]);

  const words = [{ form: "maisons", spelling: "maisons", examples: [] }, { form: "paris", spelling: "Paris", examples: ["à Paris en hiver"] }, { form: "est", spelling: "est", examples: [] }];
  check("glossary question", glossaryQuestion(words.slice(1, 2)) === "1. Paris\n   — à Paris en hiver");
  check("glossary prompt names the language", glossarySystem("Russian").includes("на языке «Russian»:\n{\"words\": [{\"n\": "));
  const answer = '```json\n{"words": [\n'
    + '{"n": 1, "lemma": "maison", "form_note": "мн. ч.", "meaning": "дома"},\n'
    + '{"n": "2", "lemma": "Paris", "form_note": "", "meaning": "Париж,\\t\\"столица\\""},\n'
    + '{"n": 9, "lemma": "x", "meaning": "вне списка"},\n'
    + '{"n": 3, "lemma": "être", "meaning": ""}\n]}\n```';
  const definitions = glossaryDefinitions(answer, words);
  check("glossary answer becomes definitions", definitions?.size === 2 && definitions.get("maisons") === "maison (мн. ч.): дома"
    && definitions.get("paris") === "Париж, 'столица'", JSON.stringify([...(definitions ?? [])]));
  check("an unreadable glossary answer is no answer", glossaryDefinitions("Sorry, I cannot help.", words) === null);
  const mocked = glossaryDefinitions(mockReply([system(glossarySystem("Russian")), user(glossaryQuestion(words))]).content ?? "", words);
  check("mock defines every word of a batch", mocked?.size === 3 && mocked.get("paris") === "«Paris» в книге (макет)");

  const env = await freshEnv();
  env.settings.saveAi({ ...env.settings.ai(), endpoint: mockEndpoint });
  const book = { id: 7, title: "Le Petit Livre", language: "fr" };
  // 302 forms: more batches than run at once, so a stop leaves some unasked.
  const letter = (n) => String.fromCharCode(97 + n);
  const chapters = [Array.from({ length: 300 }, (_, i) => `Le mot${letter(Math.floor(i / 26))}${letter(i % 26)} arrive.`).join("\n")];
  const feature = new GlossaryFeature(env, book, chapters, "fr");
  await feature.count();
  check("a new glossary counts the book's words", feature.total === 302 && feature.defined === 0 && feature.estimatedTokens === 302 * 120, `${feature.total} ${feature.defined}`);
  const stopAtFirst = feature.subscribe(() => feature.defined && feature.stop());
  await feature.start();
  stopAtFirst();
  check("a stopped run keeps what came back", feature.defined === 200 && !feature.isRunning, `${feature.defined}`);
  const again = new GlossaryFeature(env, book, chapters, "fr");
  await again.count();
  check("the next run begins where the last stopped", again.defined === feature.defined);
  await again.start();
  check("a finished glossary defines every word", again.defined === again.total && !again.error, again.error);
  const pack = await env.packs.glossary(book);
  const found = await env.dictionary.lookup("Arrive", await env.packs.enabled());
  check("lookups find the glossary", pack?.name === "Le Petit Livre glossary"
    && found.articles.some((article) => article.source === pack.name && article.senses[0] === "«arrive» в книге (макет)"), JSON.stringify(found.articles));
}
