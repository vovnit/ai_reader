// Servers that do not answer web pages (CORS) — Nextcloud's WebDAV among
// them — are reached through the site the app is served from, when that site
// relays them: the Cloudflare Worker in `relay.js`, for the hosts its owner
// listed. The site is asked once per host. Served anywhere else, and under
// Node, requests go straight to the server.

const answers = new Map();

/** The address to send a request for `url` to: the site's relay, or `url` itself. */
export async function routed(url) {
  if (typeof location === "undefined" || !/^https?:$/.test(location.protocol)) return url;
  const host = new URL(url).hostname;
  if (host === location.hostname) return url;
  if (!answers.has(host)) {
    const asked = fetch(new URL(`relay?host=${encodeURIComponent(host)}`, location.href), { cache: "no-store" });
    answers.set(host, asked.then((response) => response.status === 204, () => false));
  }
  return (await answers.get(host)) ? new URL(`relay?url=${encodeURIComponent(url)}`, location.href).href : url;
}
