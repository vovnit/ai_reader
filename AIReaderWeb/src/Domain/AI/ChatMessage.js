// One message in a chat completion exchange:
// `{ role, content, toolCalls: [{ id, name, arguments }], toolCallId }`,
// where `arguments` is a JSON object encoded as a string, as the API returns it.

export function message(role, content, extra = {}) {
  return { role, content, toolCalls: [], toolCallId: "", ...extra };
}

export const system = (content) => message("system", content);
export const user = (content) => message("user", content);
export const assistant = (content) => message("assistant", content);
export const toolResult = (content, callId) => message("tool", content, { toolCallId: callId });

/** A reply that calls one tool with `{ [argument]: value }`. */
export function toolCall(id, name, argument, value) {
  return message("assistant", null, { toolCalls: [{ id, name, arguments: JSON.stringify({ [argument]: value }) }] });
}

export function toJson(chat) {
  const json = { role: chat.role, content: chat.content ?? null };
  if (chat.toolCalls.length) {
    // Services require the type back on every call they made, even the
    // ones that leave it out of their own reply.
    json.tool_calls = chat.toolCalls.map((call) => ({
      id: call.id,
      type: "function",
      function: { name: call.name, arguments: call.arguments },
    }));
  }
  if (chat.toolCallId) json.tool_call_id = chat.toolCallId;
  return json;
}

export function fromJson(json) {
  return message(json?.role ?? "", typeof json?.content === "string" ? json.content : null, {
    toolCalls: (Array.isArray(json?.tool_calls) ? json.tool_calls : []).map((call) => {
      const args = call?.function?.arguments;
      return {
        id: call?.id ?? "",
        name: call?.function?.name ?? "",
        // Some services return the arguments as an object rather than a string.
        arguments: typeof args === "string" ? args : JSON.stringify(args ?? null),
      };
    }),
    toolCallId: typeof json?.tool_call_id === "string" ? json.tool_call_id : "",
  });
}
