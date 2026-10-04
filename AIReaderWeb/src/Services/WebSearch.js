// A web search through Monid: `POST /v1/run` names the provider's endpoint
// and its input; a provider that answers at once returns the output, and
// one that runs in the background returns a run to poll at `/v1/runs/{id}`.
import { bearer, errorMessage, send } from "./Http.js";
import { webRequest } from "./Settings.js";

const apiBase = "https://api.monid.ai/v1";
/** How long a search in the background is waited for, all polls together. */
const waitSeconds = 60;

function parsed({ status, text }) {
  if (status < 200 || status >= 300) throw new Error(`The web search failed (${status}): ${errorMessage(text)}`);
  try {
    return JSON.parse(text);
  } catch {
    throw new Error("The web search's answer could not be read.");
  }
}

const isDone = (run) => ["COMPLETED", "FAILED", "BLOCKED"].includes(run?.status);

/** The provider's data out of a finished run, or why there is none. */
function output(run) {
  if (run.status !== "COMPLETED") {
    const reason = typeof run.error === "string" ? run.error : run.error?.message ?? "";
    throw new Error(`The web search did not complete (${run.status}${reason ? `: ${reason}` : ""}).`);
  }
  const provider = run.providerResponse ?? {};
  const httpStatus = typeof provider.httpStatus === "number" ? provider.httpStatus : 200;
  if (httpStatus >= 400) {
    const reason = provider.error?.message ?? "";
    throw new Error(`The search provider answered ${httpStatus}${reason ? `: ${reason}` : "."}`);
  }
  // Every run costs something; the console says how much.
  if (run.cost && typeof run.cost === "object") {
    console.info(`web search ${run.provider}${run.endpoint} cost ${run.cost.value} ${run.cost.currency}`);
  }
  return provider.data == null ? run.output : provider.data;
}

/** Whatever JSON the endpoint returned. `language` is the book's, for endpoints that take one. */
export async function searchWeb(settings, query, language) {
  let input;
  try {
    input = JSON.parse(webRequest(settings, query, language));
  } catch {}
  if (!input || typeof input !== "object") throw new Error("The web search input is not valid JSON; check it in Settings.");
  const headers = { "Content-Type": "application/json", ...bearer(settings.apiKey) };
  let run = parsed(await send(`${apiBase}/run`, {
    method: "POST",
    headers,
    body: JSON.stringify({ provider: settings.provider, endpoint: settings.endpoint, input }),
  }));
  // A provider that works in the background is polled, a little less often each time.
  let waited = 0;
  let delay = 1;
  while (!isDone(run) && waited < waitSeconds) {
    await new Promise((resolve) => setTimeout(resolve, delay * 1000));
    waited += delay;
    delay = Math.min(delay * 1.5, 10);
    if (!run.runId) throw new Error("The web search returned no run to wait for.");
    run = parsed(await send(`${apiBase}/runs/${run.runId}`, { headers: bearer(settings.apiKey), seconds: 30 }));
  }
  if (!isDone(run)) throw new Error("The web search took too long.");
  return output(run);
}
