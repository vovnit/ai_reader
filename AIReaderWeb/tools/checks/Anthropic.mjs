// Anthropic's Messages API: the request made from chat messages and the
// reply read back — the Kindle check's cases, with the same expectations —
// and a conversation with tools against a local stand-in for the API.
import { anthropicBody, anthropicReply } from "../../src/Domain/AI/AnthropicMessages.js";
import { system, toolCall, toolResult, user } from "../../src/Domain/AI/ChatMessage.js";
import { explanationQuestion, explanationSystem, explanationTools } from "../../src/Domain/AI/ExplanationPrompt.js";
import { noEffort, quirkNamed } from "../../src/Domain/AI/RequestQuirks.js";
import { decodeExplanation } from "../../src/Domain/AI/WordExplanation.js";
import { models } from "../../src/Services/ChatApi.js";
import { chatUrl, defaultAi, modelsUrl, usesAnthropic } from "../../src/Services/Settings.js";
import { budget, converse } from "../../src/Services/ToolRunner.js";
import { createServer } from "node:http";
import { check } from "./Check.mjs";

const answer = '{"lemma":"realize","form_note":"","meaning":"понять","guessed":true,"confidence":0.4}';

/**
 * A model that thinks and calls the dictionary for as long as it may, and
 * refuses `effort` as an older model does. Each request is kept as sent.
 */
async function standIn() {
  const asks = [];
  const server = createServer((request, response) => {
    let body = "";
    request.on("data", (chunk) => (body += chunk));
    request.on("end", () => {
      const sent = body ? JSON.parse(body) : null;
      asks.push({ url: request.url, headers: request.headers, body: sent });
      response.writeHead(sent?.output_config ? 400 : 200, { "Content-Type": "application/json" });
      if (request.method === "GET") return response.end(JSON.stringify({ data: [{ id: "claude-b" }, { id: "claude-a" }], has_more: false }));
      if (sent.output_config) return response.end(JSON.stringify({ type: "error", error: { type: "invalid_request_error", message: "This model does not support the effort parameter." } }));
      const n = asks.length;
      const content = sent.tool_choice?.type === "none"
        ? [{ type: "thinking", thinking: "", signature: `s${n}` }, { type: "text", text: answer }]
        : [{ type: "thinking", thinking: "", signature: `s${n}` }, { type: "tool_use", id: `toolu_${n}`, name: "lookup_dictionary", input: { word: "realize" } }];
      response.end(JSON.stringify({ role: "assistant", content, stop_reason: sent.tool_choice ? "end_turn" : "tool_use" }));
    });
  });
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  return { asks, server, endpoint: `http://127.0.0.1:${server.address().port}/anthropic/v1` };
}

async function checkConversation() {
  const { asks, server, endpoint } = await standIn();
  const settings = { ...defaultAi, endpoint, model: "claude-old", apiKey: "k" };
  const none = { query: "realized", forms: [], articles: [] };
  const asked = [system(explanationSystem("Russian")), user(explanationQuestion("realized", "That, I realized, was my mistake.", "No entry."))];
  const reply = await converse(settings, asked, explanationTools, true, { scope: {}, packs: [], dictionary: { lookup: async () => none } });
  const listed = await models(settings);
  server.close();

  const sent = asks.filter((ask) => ask.url.endsWith("/messages"));
  const last = sent.at(-1)?.body;
  check("anthropic: the token and version travel in its own headers", sent[0]?.headers["x-api-key"] === "k" && sent[0].headers["anthropic-version"] === "2023-06-01"
    && !sent[0].headers.authorization && sent[0].url === "/anthropic/v1/messages", JSON.stringify(sent[0]?.headers));
  check("anthropic: a refused effort is dropped and the request sent again", sent[0]?.body.output_config && !sent[1]?.body.output_config && sent.length === budget + 2);
  check("anthropic: the last request keeps the tools but forbids them", last?.tools?.length === explanationTools.length && last.tool_choice?.type === "none"
    && sent.slice(1, -1).every((ask) => ask.body.tools && !ask.body.tool_choice));
  const replies = last?.messages.filter((turn) => turn.role === "assistant") ?? [];
  check("anthropic: replies go back as they came, thinking included", replies.length === budget
    && replies.every((turn, i) => turn.content[0].type === "thinking" && turn.content[0].signature === `s${i + 2}`), JSON.stringify(replies));
  check("anthropic: tool results go back in a user turn", last?.messages[2]?.role === "user" && last.messages[2].content[0].type === "tool_result"
    && last.messages[2].content[0].tool_use_id === "toolu_2");
  check("anthropic: the answer is read", decodeExplanation(reply.content ?? "")?.meaning === "понять", reply.content);
  check("anthropic: models are listed, all of them", listed.join() === "claude-a,claude-b" && asks.at(-1).url === "/anthropic/v1/models?limit=1000");
}

export async function checkAnthropic() {
  check("anthropic: an endpoint naming it speaks its API", usesAnthropic({ endpoint: "https://api.anthropic.com/v1" })
    && chatUrl({ endpoint: "https://api.anthropic.com/v1/" }) === "https://api.anthropic.com/v1/messages"
    && modelsUrl({ endpoint: "https://api.anthropic.com/v1" }) === "https://api.anthropic.com/v1/models?limit=1000"
    && !usesAnthropic(defaultAi) && chatUrl(defaultAi) === "https://api.mistral.ai/v1/chat/completions");

  const call = toolCall("c1", "search_book", "query", "Meaulnes");
  const messages = [system("Be brief."), user("Who?"), call, toolResult("Two passages.", "c1"), toolResult("Nothing.", "c1"), user("And?")];
  const body = anthropicBody("claude-x", messages, explanationTools, true, 700, new Set());
  check("anthropic: the system prompt stands apart", body.system === "Be brief." && body.messages[0].role === "user" && body.messages[0].content[0].text === "Who?");
  check("anthropic: a tool call becomes a tool_use", body.messages[1].content.length === 1 && body.messages[1].content[0].type === "tool_use"
    && body.messages[1].content[0].input.query === "Meaulnes");
  check("anthropic: neighbouring user messages are one turn", body.messages.length === 3 && body.messages[2].content.length === 3
    && body.messages[2].content[2].text === "And?", JSON.stringify(body.messages));
  check("anthropic: tools in its shape, effort low, no temperature", body.tools[0].name === explanationTools[0].function.name
    && body.tools[0].input_schema.required.length === 1 && body.output_config.effort === "low" && !("temperature" in body) && !body.tool_choice && body.max_tokens === 700);
  check("anthropic: no effort once refused", !anthropicBody("m", messages, [], true, 1, new Set([noEffort])).output_config);
  check("anthropic: a refused effort is read from the message", quirkNamed('{"type":"error","error":{"type":"invalid_request_error","message":"output_config.effort: not supported"}}') === noEffort
    && quirkNamed('{"type":"error","error":{"type":"invalid_request_error","message":"max_tokens: too large"}}') === null);

  const read = anthropicReply({ content: [{ type: "thinking", thinking: "", signature: "x" }, { type: "text", text: "Un " }, { type: "text", text: "mot." },
    { type: "tool_use", id: "t1", name: "lookup_dictionary", input: { word: "mot" } }] });
  check("anthropic: a reply's text, calls and blocks", read.content === "Un mot." && read.toolCalls[0].arguments === '{"word":"mot"}' && read.blocks.length === 4
    && anthropicBody("m", [read], [], true, 1, new Set()).messages[0].content === read.blocks);
  let declined = "";
  try {
    anthropicReply({ content: [], stop_reason: "refusal" });
  } catch (error) {
    declined = error.message;
  }
  check("anthropic: a refusal is told", declined === "The model declined to answer." && anthropicReply({}) === null);
  await checkConversation();
}
