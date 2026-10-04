// Chat completions and the model list, against any OpenAI-compatible
// service. Failures throw with a message fit to show.
import { fromJson, toJson } from "../Domain/AI/ChatMessage.js";
import { mockModels, mockReply } from "../Domain/AI/MockAI.js";
import {
  completionTokens, defaultTemperature, learnQuirk, noReasoning, quirkCount, quirkNamed, quirksFor,
} from "../Domain/AI/RequestQuirks.js";
import { bearer, errorMessage, send } from "./Http.js";
import { aiToken, chatUrl, modelsUrl, usesMock } from "./Settings.js";

function requestBody(settings, messages, tools, jsonMode, quirks) {
  const body = { model: settings.model, messages: messages.map(toJson) };
  body[quirks.has(completionTokens) ? "max_completion_tokens" : "max_tokens"] = 700;
  if (!quirks.has(defaultTemperature)) body.temperature = 0.2;
  if (quirks.has(noReasoning)) body.reasoning_effort = "none";
  if (tools.length) {
    body.tools = tools;
    body.tool_choice = "auto";
  }
  if (jsonMode) body.response_format = { type: "json_object" };
  return JSON.stringify(body);
}

function checkEndpoint(settings) {
  if (!settings.endpoint.includes("://")) throw new Error(`“${settings.endpoint}” is not a valid endpoint URL.`);
}

export async function chat(settings, messages, tools = [], jsonMode = false) {
  if (usesMock(settings)) return mockReply(messages);
  checkEndpoint(settings);
  const signature = `${settings.endpoint}|${settings.model}`;
  const quirks = quirksFor(signature);
  // A service that rejects a parameter says which one, so drop or rename it
  // and try again rather than failing the lookup.
  for (let attempt = 0; attempt <= quirkCount; attempt++) {
    const { status, text } = await send(chatUrl(settings), {
      method: "POST",
      headers: { "Content-Type": "application/json", ...bearer(aiToken(settings)) },
      body: requestBody(settings, messages, tools, jsonMode, quirks),
    });
    if (status === 400) {
      const quirk = quirkNamed(text);
      if (quirk && !quirks.has(quirk)) {
        quirks.add(quirk);
        learnQuirk(quirk, signature);
        continue;
      }
    }
    if (status < 200 || status >= 300) throw new Error(`The request failed (${status}): ${errorMessage(text)}`);
    let json;
    try {
      json = JSON.parse(text);
    } catch {}
    if (!json?.choices?.length) throw new Error("The service returned no answer.");
    return fromJson(json.choices[0].message);
  }
  throw new Error("The service returned no answer.");
}

/** The model names the endpoint offers, sorted. */
export async function models(settings) {
  if (usesMock(settings)) return [...mockModels];
  checkEndpoint(settings);
  const { status, text } = await send(modelsUrl(settings), { headers: bearer(aiToken(settings)), seconds: 20 });
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
