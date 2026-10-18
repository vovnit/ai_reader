// Chat completions and the model list, against any OpenAI-compatible
// service, or Anthropic's in its own Messages API. Failures throw with a
// message fit to show.
import { anthropicBody, anthropicReply, anthropicVersion } from "../Domain/AI/AnthropicMessages.js";
import { fromJson, toJson } from "../Domain/AI/ChatMessage.js";
import { mockModels, mockReply } from "../Domain/AI/MockAI.js";
import { completionTokens, defaultTemperature, noReasoning, quirkCount, quirkNamed } from "../Domain/AI/RequestQuirks.js";
import { bearer, errorMessage, send } from "./Http.js";
import { forgetQuirks, learnQuirk, quirksFor } from "./RequestQuirkStore.js";
import { aiToken, chatUrl, modelsUrl, usesAnthropic, usesMock } from "./Settings.js";

/** Enough for a lookup. */
export const lookupTokens = 700;

function requestBody(settings, messages, tools, toolsAllowed, jsonMode, maxTokens, quirks) {
  if (usesAnthropic(settings)) return JSON.stringify(anthropicBody(settings.model, messages, tools, toolsAllowed, maxTokens, quirks));
  const body = { model: settings.model, messages: messages.map(toJson) };
  body[quirks.has(completionTokens) ? "max_completion_tokens" : "max_tokens"] = maxTokens;
  if (!quirks.has(defaultTemperature)) body.temperature = 0.2;
  if (quirks.has(noReasoning)) body.reasoning_effort = "none";
  if (tools.length && toolsAllowed) {
    body.tools = tools;
    body.tool_choice = "auto";
  }
  if (jsonMode) body.response_format = { type: "json_object" };
  return JSON.stringify(body);
}

/** Anthropic takes the token in a header of its own, and answers a page only when told the page holds the reader's own key. */
function authorization(settings) {
  const token = aiToken(settings);
  if (!usesAnthropic(settings)) return bearer(token);
  return { ...(token ? { "x-api-key": token } : {}), "anthropic-version": anthropicVersion, "anthropic-dangerous-direct-browser-access": "true" };
}

function checkEndpoint(settings) {
  if (!settings.endpoint.includes("://")) throw new Error(`“${settings.endpoint}” is not a valid endpoint URL.`);
}

/**
 * `maxTokens` bounds the answer. `toolsAllowed` false keeps the tools listed
 * but lets the model only answer, for the last request of a conversation
 * that has used them.
 */
export async function chat(settings, messages, tools = [], jsonMode = false, maxTokens = lookupTokens, toolsAllowed = true) {
  if (usesMock(settings)) return mockReply(messages);
  checkEndpoint(settings);
  const signature = `${settings.endpoint}|${settings.model}`;
  const quirks = quirksFor(signature);
  // A service that rejects a parameter says which one, so drop or rename it
  // and try again rather than failing the lookup.
  for (let attempt = 0; attempt <= quirkCount; attempt++) {
    const { status, text } = await send(chatUrl(settings), {
      method: "POST",
      headers: { "Content-Type": "application/json", ...authorization(settings) },
      body: requestBody(settings, messages, tools, toolsAllowed, jsonMode, maxTokens, quirks),
      // A longer answer takes longer to write.
      seconds: Math.max(45, maxTokens / 20),
    });
    if (status === 400) {
      const quirk = quirkNamed(text);
      if (quirk && !quirks.has(quirk)) {
        quirks.add(quirk);
        learnQuirk(quirk, signature);
        continue;
      }
      // Refused over something already adjusted: what was remembered no
      // longer fits, so the next request finds out afresh.
      if (quirk) forgetQuirks(signature);
    }
    if (status < 200 || status >= 300) throw new Error(`The request failed (${status}): ${errorMessage(text)}`);
    let json;
    try {
      json = JSON.parse(text);
    } catch {}
    const reply = usesAnthropic(settings) ? anthropicReply(json) : json?.choices?.length ? fromJson(json.choices[0].message) : null;
    if (!reply) throw new Error("The service returned no answer.");
    return reply;
  }
  throw new Error("The service returned no answer.");
}

/** The model names the endpoint offers, sorted. */
export async function models(settings) {
  if (usesMock(settings)) return [...mockModels];
  checkEndpoint(settings);
  const { status, text } = await send(modelsUrl(settings), { headers: authorization(settings), seconds: 20 });
  if (status < 200 || status >= 300) throw new Error(`The request failed (${status}): ${errorMessage(text)}`);
  let json;
  try {
    json = JSON.parse(text);
  } catch {
    throw new Error("The model list could not be read.");
  }
  const names = (json?.data ?? []).map((model) => model?.id).filter((id) => typeof id === "string");
  return [...new Set(names)].sort();
}
