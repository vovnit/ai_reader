// The model's side: the dictionary summary, the prompts, the mock, the web
// search, the request quirks and the book search — the Kindle check's
// cases, with the same expectations.
import { fromJson, system, toJson, toolResult, user } from "../../src/Domain/AI/ChatMessage.js";
import { chatMessages, chatTools, pageContext, wordContext } from "../../src/Domain/AI/ChatPrompt.js";
import { explanationQuestion, explanationSystem, explanationTools } from "../../src/Domain/AI/ExplanationPrompt.js";
import { markdownLines } from "../../src/Domain/AI/ChatMarkdown.js";
import { mockReply } from "../../src/Domain/AI/MockAI.js";
import { completionTokens, defaultTemperature, learnQuirk, noReasoning, quirkNamed, quirksFor } from "../../src/Domain/AI/RequestQuirks.js";
import { passagesSummary, webHits, webSummary } from "../../src/Domain/AI/SearchSummary.js";
import { argument, dictionaryToolName, searchToolName, webSearchToolName } from "../../src/Domain/AI/Tools.js";
import { decodeExplanation } from "../../src/Domain/AI/WordExplanation.js";
import { xrayMessages } from "../../src/Domain/AI/XRayPrompt.js";
import { lookupSummary } from "../../src/Domain/Dictionary/DictionaryLookup.js";
import { normalizeWord } from "../../src/Domain/Dictionary/WordNormalizer.js";
import { selectionAt } from "../../src/Domain/Reading/WordContext.js";
import { findInText } from "../../src/Domain/Search/BookSearch.js";
import { defaultAi, defaultWeb, mockEndpoint, webConfigured, webRequest } from "../../src/Services/Settings.js";
import { converse } from "../../src/Services/ToolRunner.js";
import { check } from "./Check.mjs";

const sampleLookup = () => ({
  query: "maisons",
  forms: [{ lemma: "maison", partOfSpeech: "NOM", gender: "f", number: "p", features: [] }],
  articles: [{ lemma: "maison", partOfSpeech: "noun", senses: ["дом", "здание"], source: "Bundled" }],
});

export async function checkAi() {
  const summary = lookupSummary(sampleLookup());
  check("lookup summary format", summary === "Dictionary results for “maisons”:\n- form of “maison” (NOM f p)\n- maison [noun]: дом; здание", summary);
  check("empty lookup summary", lookupSummary({ query: "xyz", forms: [], articles: [] }) === "No dictionary entry for “xyz”.");
  check("normalizer", normalizeWord("«L’Été,»") === "l'été" && normalizeWord("Paris.") === "paris", normalizeWord("«L’Été,»"));

  const messages = [system(explanationSystem("Russian")), user(explanationQuestion("maisons", "Les maisons étaient vieilles.", summary))];
  const reply = mockReply(messages);
  const explanation = decodeExplanation(reply.content ?? "");
  check("mock answers from the article", explanation?.lemma === "maison" && explanation.meaning === "дом" && !explanation.guessed, reply.content);
  check("mock form note mentions grammar", explanation?.formNote.includes("NOM f p"));
  const none = { query: "zut", forms: [], articles: [] };
  const empty = [system(explanationSystem("Russian")), user(explanationQuestion("zut", "Zut alors.", lookupSummary(none)))];
  const call = mockReply(empty);
  check("mock asks the dictionary again when empty", call.toolCalls.length === 1 && call.toolCalls[0].name === dictionaryToolName);
  empty.push(call, toolResult(lookupSummary(none), call.toolCalls[0].id));
  const guessed = decodeExplanation(mockReply(empty).content ?? "");
  check("mock guesses after a second miss", guessed?.guessed && guessed.confidence < 0.5);
  check("messages serialize with content", toJson(messages[1]).role === "user" && typeof toJson(messages[1]).content === "string");
  check("tool calls serialize with type", toJson(call).tool_calls[0].type === "function");
  check("tool calls round-trip", fromJson(toJson(call)).toolCalls[0].arguments === call.toolCalls[0].arguments);

  const exa = { results: [{ title: "Le Grand Meaulnes", url: "https://fr.wikipedia.org/wiki/Le_Grand_Meaulnes", text: "Roman d'Alain-Fournier.\nParu en 1913." }, { name: "Sans adresse", snippet: "Un extrait." }] };
  const hits = webHits(exa);
  check("web hits read title, url and text", hits.length === 2 && hits[0].text === "Roman d'Alain-Fournier. Paru en 1913." && hits[1].title === "Sans adresse");
  check("web hits from a bare list", webHits([{ link: "https://a.example", description: "A" }]).length === 1);
  check("web hits from a nested list", webHits({ data: { organic: [{ title: "B" }] } }).length === 1);
  const web = webSummary("Meaulnes", exa);
  check("web summary numbers the pages", web.startsWith("Pages found for “Meaulnes”:\n1. Le Grand Meaulnes — https://") && web.includes("\n2. Sans adresse\n   Un extrait."), web);
  check("web summary says when nothing was found", webSummary("x", null) === "Nothing was found on the web for “x”." && webSummary("x", []) === "Nothing was found on the web for “x”.");
  check("web summary hands over an unknown shape", webSummary("x", { answer: "42" }).includes('{"answer":"42"}'));
  const cut = webHits([{ title: "T", text: "a".repeat(2000) }]);
  check("web hits cut a long text", cut[0].text.length < 700 && cut[0].text.endsWith("…"));
  check("web search is off until a key is given", !webConfigured(defaultWeb) && defaultWeb.provider === "tinyfish");
  const settings = { ...defaultWeb, apiKey: "k" };
  const request = JSON.parse(webRequest(settings, 'dit "non"\nvite', "fr"));
  check("web search request puts the query and language in, escaped", webConfigured(settings) && request.queryParams.query === 'dit "non"\nvite' && request.queryParams.language === "fr");
  check("web search request falls back to English", JSON.parse(webRequest(settings, "x", "")).queryParams.language === "en");
  const asked = chatMessages(pageContext("Page."), [{ isReader: true, text: "Поищи Alain-Fournier" }], "Russian");
  const webCall = mockReply(asked);
  check("mock chat calls search_web", webCall.toolCalls[0]?.name === webSearchToolName && argument(webCall.toolCalls[0].arguments, "query") === "Alain-Fournier");
  const conversation = [...asked];
  const answer = await converse({ ...defaultAi, endpoint: mockEndpoint }, conversation, chatTools, false, { scope: {}, packs: [] });
  check("tool runner offers and answers the web under the mock", conversation.length === 4 && conversation[3].role === "tool"
    && conversation[3].content.startsWith("Pages found for “Alain-Fournier”") && answer.content.includes("2 страниц"), answer.content);

  check("quirk: max_tokens", quirkNamed('{"error":{"message":"Unsupported parameter","param":"max_tokens"}}') === completionTokens);
  check("quirk: temperature", quirkNamed('{"error":{"message":"x","param":"temperature"}}') === defaultTemperature);
  check("quirk: reasoning", quirkNamed('{"error":{"message":"x","param":"reasoning_effort"}}') === noReasoning);
  check("quirk: unrelated", !quirkNamed('{"error":{"message":"nope"}}') && !quirkNamed("not json"));
  learnQuirk(noReasoning, "m");
  check("quirk store remembers", quirksFor("m").has(noReasoning));

  const chat = chatMessages(pageContext("Page text."), [{ isReader: true, text: "Q1" }, { isReader: false, text: "A1" }, { isReader: true, text: "Q2" }], "Russian");
  check("chat sends the page once", chat.length === 4 && chat[1].content.includes("Page text.") && chat[3].content === "Q2");
  check("chat keeps the model's turns", chat[2].role === "assistant" && chat[2].content === "A1");
  check("chat answers in the chosen language", chatMessages("", [{ isReader: true, text: "Q" }], "English")[0].content.includes("«English»") && explanationSystem("English").includes("«English»"));
  const word = wordContext("maisons", "Les maisons.", { lemma: "maison", formNote: "мн. ч.", meaning: "дом", guessed: false, confidence: 0.9 });
  check("a word seeds a conversation with its explanation", word.includes("Слово: maisons") && word.includes("Объяснение: дом"));
  const echo = mockReply(chatMessages(word, [{ isReader: true, text: "Почему?" }], "Russian"));
  check("mock tells a seeded conversation from a lookup", !echo.toolCalls.length && echo.content.includes("«Почему?»"), echo.content);
  const find = chatMessages(pageContext("Page."), [{ isReader: true, text: "Найди Meaulnes" }], "Russian");
  const searchCall = mockReply(find);
  check("mock chat calls search_book", searchCall.toolCalls[0]?.name === searchToolName && argument(searchCall.toolCalls[0].arguments, "query") === "Meaulnes");
  const hit = { bookTitle: "", chapter: 0, excerpt: "Meaulnes entra." };
  find.push(searchCall, toolResult(passagesSummary("Meaulnes", [hit, hit], false), searchCall.toolCalls[0].id));
  check("mock chat counts the passages", mockReply(find).content.includes("2 отрывков"));
  const meaning = chatMessages(pageContext("Page."), [{ isReader: true, text: "Что значит maison?" }], "Russian");
  const lookupCall = mockReply(meaning);
  check("mock chat calls lookup_dictionary", lookupCall.toolCalls[0]?.name === dictionaryToolName && argument(lookupCall.toolCalls[0].arguments, "word") === "maison");
  meaning.push(lookupCall, toolResult("Dictionary results for “maison”:\n- maison [noun]: дом, здание; семья", lookupCall.toolCalls[0].id));
  check("mock chat answers from the article", mockReply(meaning).content.includes("«maison» — дом, здание"));
  check("chat offers the dictionary and the search", chatTools.length === 2 && explanationTools[1].function.name === searchToolName);

  const text = "Le grand Meaulnes arriva un dimanche. Il pleuvait.\nMeaulnes, lui, ne dit rien! Puis il partit.";
  const found = findInText(text, "meaulnes", 3, 10);
  check("search is case-insensitive and finds every hit", found.length === 2 && found[0].chapter === 3 && found[1].offset === text.indexOf("Meaulnes, lui"));
  check("excerpt is the sentence", found[0]?.excerpt === "Le grand Meaulnes arriva un dimanche." && found[1]?.excerpt === "Meaulnes, lui, ne dit rien!");
  check("excerpt marks the match", found[0].excerpt.slice(found[0].matchStart, found[0].matchEnd) === "Meaulnes" && found[1].matchStart === 0);
  check("search respects the limit", findInText(text, "l", 0, 3).length === 3);
  check("one hit per sentence", findInText("Un chat, deux chats. Trois chats.", "chat", 0, 9).length === 2);
  check("search: accented letters match by case", findInText("ÉTÉ. Puis été.", "été", 0, 9).length === 2);
  check("nothing for an empty query", !findInText(text, "", 0, 9).length && !findInText(text, "zzz", 0, 9).length);
  const longer = `Début ${"a".repeat(400)} milieu cible ${"a".repeat(400)} fin.`;
  const long = findInText(longer, "cible", 0, 1)[0];
  check("a long sentence is cut around the match, at word boundaries", long.excerpt.startsWith("…") && long.excerpt.endsWith("…")
    && long.excerpt.length < 600 && long.excerpt.slice(long.matchStart, long.matchEnd) === "cible");
  check("search summary names book and chapter", passagesSummary("vint", [{ bookTitle: "Tome 2", chapter: 4, excerpt: "Il vint." }], true).includes("1. [Tome 2, chapter 5] Il vint."));
  check("empty search summary", passagesSummary("x", [], false).includes("No passage"));
  const xray = xrayMessages("Meaulnes", [], false, "Russian");
  const again = mockReply(xray);
  check("mock x-ray searches when given no passages", argument(again.toolCalls[0]?.arguments ?? "", "query") === "meaulnes");
  xray.push(again, toolResult(passagesSummary("meaulnes", [hit, hit, hit], false), again.toolCalls[0].id));
  check("mock x-ray answers from the passages", mockReply(xray).content.includes("3 отрывках"));

  const sentenceText = "Les maisons étaient vieilles. Il pleuvait fort.\nNouveau paragraphe ici.";
  const selection = selectionAt(sentenceText, sentenceText.indexOf("aisons"), "fr");
  check("tap resolves the word", selection?.word === "maisons", selection?.word);
  check("tap resolves the sentence", selection?.sentence === "Les maisons étaient vieilles.", selection?.sentence);
  check("second sentence", selectionAt(sentenceText, sentenceText.indexOf("pleuvait"), "fr")?.sentence === "Il pleuvait fort.");
  check("a tap on a space is not a word", !selectionAt(sentenceText, sentenceText.indexOf(" étaient"), "fr"));
  check("paragraph break ends the sentence", selectionAt(sentenceText, sentenceText.indexOf("paragraphe"), "fr")?.sentence === "Nouveau paragraphe ici.");

  const lines = markdownLines("# Titre\n- **gras** et *penché*, `code`\n> cité\n```\nx < y\n```\n1. un");
  check("markdown lines", lines.length === 5 && lines[0].runs[0].bold && lines[1].prefix === "• " && lines[1].runs[0].bold
    && lines[1].runs[2].italic && lines[1].runs[4].code && lines[2].kind === "quote" && lines[3].runs[0].text === "x < y" && lines[4].runs[0].text === "1. un",
    JSON.stringify(lines));
}
