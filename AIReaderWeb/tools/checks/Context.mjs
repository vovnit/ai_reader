// `expand_context`: reading a step at a time before or after a passage,
// within its chapter, and the mock and the runner using it.
import { chatMessages, chatTools, pageContext } from "../../src/Domain/AI/ChatPrompt.js";
import { contextDirection, nothingAround, readAround } from "../../src/Domain/AI/ContextReading.js";
import { contextTool, contextToolName } from "../../src/Domain/AI/Tools.js";
import { reach, step } from "../../src/Domain/Search/BookSearch.js";
import { BookCorpus } from "../../src/Services/BookCorpus.js";
import { defaultAi, mockEndpoint } from "../../src/Services/Settings.js";
import { converse } from "../../src/Services/ToolRunner.js";
import { check } from "./Check.mjs";

export async function checkContext() {
  let chapter = "";
  for (let i = 0; i < 60; i++) chapter += `Phrase ${i} dit une chose assez longue pour compter. `;
  const start = chapter.indexOf("Phrase 30 ");
  const end = chapter.indexOf("Phrase 31 ");
  const window = { bookId: 1, chapter: 0, start, end };
  const before = readAround("before", chapter, window);
  check("context reads back from a sentence's start", before.startsWith("Before it in the book:\nPhrase ")
    && before.endsWith("Phrase 29 dit une chose assez longue pour compter.")
    && window.start < start && start - window.start <= step + reach, before);
  const after = readAround("after", chapter, window);
  check("context reads on to a sentence's end", after.startsWith("After it in the book:\nPhrase 31 ") && after.endsWith("compter.") && window.end > end, after);
  const reached = window.start;
  readAround("before", chapter, window);
  check("each call reads further", window.start < reached);
  let last = "";
  for (let i = 0; i < 4; i++) last = readAround("before", chapter, window);
  check("context stops at the chapter's start", window.start === 0 && last === "Nothing comes before it: the chapter begins there.", last);
  const early = { start: chapter.indexOf("Phrase 2 "), end: chapter.indexOf("Phrase 3 ") };
  const first = readAround("before", chapter, early);
  check("context says when it reached the start", first.startsWith("Before it in the book:\nPhrase 0 ") && first.endsWith("(The chapter begins here.)"), first);
  const middle = { start: 1500, end: 1510 };
  readAround("after", "x".repeat(3000), middle);
  check("a text with no sentences is still read a step at a time", middle.end > 1510 && middle.end <= 1510 + step);
  const nearEnd = { start: 2000, end: 2100 };
  check("a step near the end takes the rest", readAround("after", "x".repeat(3000), nearEnd).endsWith("(The chapter ends here.)") && nearEnd.end === 3000);

  check("context tool takes a direction", contextDirection('{"direction":"after"}') === "after" && contextDirection('{"direction":"later"}') === null
    && contextTool.function.parameters.properties.direction.enum.length === 2);

  // Asked what came before, the mock reads around the page, and the runner
  // answers from the passage the reader is on.
  const corpus = new BookCorpus([{ id: 1, title: "Tome 1" }], null);
  corpus.provide(1, ["Intro.", chapter]);
  const scope = { corpus, upTo: { bookId: 1, chapter: 1, offset: end }, passage: { bookId: 1, chapter: 1, start, end } };
  const mock = { ...defaultAi, endpoint: mockEndpoint };
  const asked = chatMessages(pageContext("Page."), [{ isReader: true, text: "Раньше что было?" }], "Russian");
  const answer = await converse(mock, asked, chatTools, false, { scope, packs: [] });
  check("mock chat reads before the passage", asked.length === 4 && asked[2].toolCalls[0]?.name === contextToolName
    && asked[3].content.startsWith("Before it in the book:") && answer.content.includes("перед этим местом в книге — «Phrase"), answer.content);
  check("the passage itself is left as it was", scope.passage.start === start);
  const later = chatMessages(pageContext("Page."), [{ isReader: true, text: "Дальше?" }], "Russian");
  await converse(mock, later, chatTools, false, { scope: { corpus }, packs: [] });
  check("without a passage there is nothing around", later.length === 4 && later[3].content === nothingAround);
}
