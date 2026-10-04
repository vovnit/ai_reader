// One HTTP request, its failures told in words a reader can act on. A page
// can reach only services that accept requests from it (CORS), or that the
// site it is served from relays; the browser reports a refusal no
// differently from a server that is down.
import { routed } from "./Relay.js";

/** The response, whatever its status; throws when nothing came back. */
export async function fetchResponse(url, { method = "GET", headers = {}, body, seconds = 45 } = {}) {
  try {
    const target = await routed(url);
    return await fetch(target, { method, headers, body, credentials: "omit", signal: AbortSignal.timeout(seconds * 1000) });
  } catch (error) {
    const host = URL.canParse(url) ? new URL(url).host : url;
    if (error?.name === "TimeoutError") throw new Error(`${host} did not answer in time.`);
    throw new Error(`${host} could not be reached. It may be offline, or it may not accept requests from a web page (CORS).`);
  }
}

/** `{ status, text }`. */
export async function send(url, options) {
  const response = await fetchResponse(url, options);
  return { status: response.status, text: await response.text() };
}

export function bearer(token) {
  return token ? { Authorization: `Bearer ${token}` } : {};
}

/** The error message inside a JSON error body, or the body itself. */
export function errorMessage(text) {
  try {
    const json = JSON.parse(text);
    const message = json?.error?.message ?? json?.message ?? json?.error;
    if (typeof message === "string" && message) return message;
  } catch {}
  return text.slice(0, 500);
}
