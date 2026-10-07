// The Worker that relays requests to servers that do not answer pages, run
// under Node against a stand-in server: what it lets through, what it keeps
// back, and which hosts it refuses.
import { createServer } from "node:http";
import relay from "../../relay.js";
import { routed } from "../../src/Services/Relay.js";
import { check } from "./Check.mjs";

export async function checkRelay() {
  const seen = [];
  const server = createServer((request, response) => {
    const chunks = [];
    request.on("data", (chunk) => chunks.push(chunk));
    request.on("end", () => {
      seen.push({ method: request.method, url: request.url, headers: request.headers, body: Buffer.concat(chunks).toString() });
      response.writeHead(request.method === "PROPFIND" ? 207 : 401, {
        "Content-Type": "application/xml",
        "Set-Cookie": "session=secret",
        "WWW-Authenticate": 'Basic realm="nextcloud"',
      });
      response.end("<multistatus/>");
    });
  });
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const base = `http://127.0.0.1:${server.address().port}`;
  const env = { RELAY_HOSTS: "cloud.example.com, 127.0.0.1" };
  const site = "https://reader.example.org";
  const ask = (query, init) => relay.fetch(new Request(`${site}/relay?${query}`, init), env);

  check("the relay says which hosts it relays", (await ask("host=127.0.0.1")).status === 204 && (await ask("host=evil.example")).status === 403);
  check("the web search's broker is relayed without being listed", (await relay.fetch(new Request(`${site}/relay?host=api.monid.ai`), {})).status === 204);
  const listing = await ask(`url=${encodeURIComponent(`${base}/remote.php/dav/files/me/AIReader/Books/`)}`, {
    method: "PROPFIND",
    headers: { Authorization: "Basic dTpw", Depth: "1", Cookie: "page=1", Origin: site },
  });
  const forwarded = seen.at(-1);
  check("the relay carries the request over: method, path, password, depth", listing.status === 207 && forwarded.method === "PROPFIND"
    && forwarded.url === "/remote.php/dav/files/me/AIReader/Books/" && forwarded.headers.authorization === "Basic dTpw" && forwarded.headers.depth === "1");
  check("the page's cookies and origin stay behind", !forwarded.headers.cookie && !forwarded.headers.origin);
  check("the answer comes back without the server's cookies", (await listing.text()) === "<multistatus/>"
    && listing.headers.get("content-type") === "application/xml" && !listing.headers.has("set-cookie") && listing.headers.get("cache-control") === "no-store");
  const refused = await ask(`url=${encodeURIComponent(`${base}/remote.php/dav/files/me/aireader-sync.json`)}`, { method: "PUT", body: '{"version":1}' });
  check("a body is carried over, and a refusal comes back without a password prompt", refused.status === 401
    && seen.at(-1).body === '{"version":1}' && !refused.headers.has("www-authenticate"));
  const before = seen.length;
  check("hosts not listed are refused, and never reached", (await ask(`url=${encodeURIComponent("https://evil.example/x")}`)).status === 403 && seen.length === before);
  check("only the relay address is relayed", (await relay.fetch(new Request(`${site}/elsewhere`), env)).status === 404
    && (await ask("url=not%20a%20url")).status === 400);
  check("served outside a site, requests go straight to the server", (await routed("https://cloud.example.com/dav")) === "https://cloud.example.com/dav");
  server.close();
}
