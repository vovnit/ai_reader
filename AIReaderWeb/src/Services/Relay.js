// Servers that do not answer web pages (CORS) — Nextcloud's WebDAV among
// them — are reached through the site the app is served from, when that site
// relays them: the Cloudflare Worker in `relay.js`, for the hosts its owner
// listed. The site is asked once per host. Served anywhere else, and under
// Node, requests go straight to the server.

/** Host → the site's answer to whether it relays it: 204 yes, 403 no, 0 when there is no relay to ask. */
const answers = new Map();

/** The address to send a request for `url` to: the site's relay, or `url` itself. */
export async function routed(url) {
  if (typeof location === "undefined" || !/^https?:$/.test(location.protocol)) return url;
  const host = new URL(url).hostname;
  if (host === location.hostname) return url;
  if (!answers.has(host)) {
    const asked = fetch(new URL(`relay?host=${encodeURIComponent(host)}`, location.href), { cache: "no-store" });
    answers.set(host, asked.then((response) => response.status, () => 0));
  }
  return (await answers.get(host)) === 204 ? new URL(`relay?url=${encodeURIComponent(url)}`, location.href).href : url;
}

/** Whether the site has a relay that leaves `url`'s host out, so adding the host would reach it. */
export async function unlisted(url) {
  return (await answers.get(new URL(url).hostname)) === 403;
}
