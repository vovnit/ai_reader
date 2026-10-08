// Parameters some OpenAI-compatible services reject while others require
// them. A service names the offending parameter in its error, so a request
// is adjusted and sent again rather than special-casing a provider; what a
// model needed is remembered by `Services/RequestQuirkStore.js`.

/** Newer OpenAI reasoning models take `max_completion_tokens` in place of `max_tokens`. */
export const completionTokens = "completionTokens";
/** Those models also only accept their default temperature. */
export const defaultTemperature = "defaultTemperature";
/** Some of them refuse function tools unless reasoning is switched off. */
export const noReasoning = "noReasoning";
export const quirkCount = 3;

/** The quirk a failed request's error body describes, or null. */
export function quirkNamed(errorBody) {
  let json;
  try {
    json = JSON.parse(errorBody);
  } catch {
    return null;
  }
  const parameter = json?.error?.param ?? "";
  const message = json?.error?.message ?? "";
  if (parameter === "max_tokens" || (typeof message === "string" && message.includes("max_completion_tokens"))) {
    return completionTokens;
  }
  if (parameter === "temperature") return defaultTemperature;
  if (parameter === "reasoning_effort") return noReasoning;
  return null;
}
