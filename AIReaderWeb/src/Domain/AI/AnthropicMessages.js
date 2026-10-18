// Anthropic's Messages API, which its endpoint speaks in place of chat
// completions: the request made from the same messages and tools
// (`ChatMessage.js`, `Tools.js`), and the reply read back into a message.
// A reply keeps its content blocks as they came, thinking included, and
// they go back unchanged in the requests that follow: the API checks that
// they do.
import { message } from "./ChatMessage.js";
import { noEffort } from "./RequestQuirks.js";

/** The API version every request names. */
export const anthropicVersion = "2023-06-01";

function input(args) {
  try {
    const parsed = JSON.parse(args);
    if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) return parsed;
  } catch {}
  return {};
}

function blocks(chat) {
  if (chat.blocks) return chat.blocks;
  if (chat.toolCallId) return [{ type: "tool_result", tool_use_id: chat.toolCallId, content: chat.content ?? "" }];
  const text = chat.content ? [{ type: "text", text: chat.content }] : [];
  return [...text, ...chat.toolCalls.map((call) => ({ type: "tool_use", id: call.id, name: call.name, input: input(call.arguments) }))];
}

/** Tool results travel in a user turn and the roles must alternate, so neighbours of one role are joined. */
function turns(messages) {
  const turns = [];
  for (const chat of messages) {
    if (chat.role === "system") continue;
    const role = chat.role === "assistant" ? "assistant" : "user";
    const content = blocks(chat);
    if (!content.length) continue;
    const last = turns.at(-1);
    if (last?.role === role) last.content = [...last.content, ...content];
    else turns.push({ role, content });
  }
  return turns;
}

/** The request body; `toolsAllowed` is false for the request that must be answered in words. */
export function anthropicBody(model, messages, tools, toolsAllowed, maxTokens, quirks) {
  const body = { model, max_tokens: maxTokens };
  const system = messages.filter((chat) => chat.role === "system" && chat.content).map((chat) => chat.content);
  if (system.length) body.system = system.join("\n\n");
  body.messages = turns(messages);
  // Newer models think before answering, and the thinking counts against
  // `max_tokens`: a lookup wants little of it.
  if (!quirks.has(noEffort)) body.output_config = { effort: "low" };
  if (tools.length) {
    body.tools = tools.map(({ function: tool }) => ({ name: tool.name, description: tool.description, input_schema: tool.parameters }));
    // A conversation that has called the tools must still list them, so
    // they are kept and only their use is forbidden.
    if (!toolsAllowed) body.tool_choice = { type: "none" };
  }
  return body;
}

/** The reply as a message, or null when it holds none; a refusal throws. */
export function anthropicReply(json) {
  if (json?.stop_reason === "refusal") throw new Error("The model declined to answer.");
  if (!Array.isArray(json?.content)) return null;
  const text = json.content.filter((block) => block?.type === "text").map((block) => block.text).join("");
  const toolCalls = json.content
    .filter((block) => block?.type === "tool_use")
    .map((block) => ({ id: block.id ?? "", name: block.name ?? "", arguments: JSON.stringify(block.input ?? {}) }));
  return message("assistant", text || null, { toolCalls, blocks: json.content });
}
