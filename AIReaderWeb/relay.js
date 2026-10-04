// The Cloudflare Worker beside the app's files: it relays the app's requests
// to servers that do not answer web pages (CORS) — Nextcloud's WebDAV among
// them. The page asks its own site, and the site asks the server. Only the
// hosts named in RELAY_HOSTS (a dashboard variable, comma-separated) are
// reached, so the site is no open proxy. Every other request is for the
// app's files, which Cloudflare serves before this runs.
//
//   GET  relay?host=cloud.example.com   204 when that host is relayed, 403 when not
//   ANY  relay?url=https://cloud.example.com/remote.php/dav/…
//                                       the request, made there; its answer, here

/** What a WebDAV request needs carried over; cookies and the page's own headers stay behind. */
const forwarded = ["authorization", "content-type", "depth", "destination", "overwrite"];
/** What comes back, minus the server's cookies and its password challenge, which would make the browser ask for a password itself. */
const returned = ["content-type", "etag", "last-modified", "dav"];

function hosts(env) {
  return new Set((env.RELAY_HOSTS ?? "").split(/[\s,]+/).filter(Boolean).map((host) => host.toLowerCase()));
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (!url.pathname.endsWith("/relay")) return new Response("Not found", { status: 404 });
    const allowed = hosts(env);
    const asked = url.searchParams.get("host");
    if (asked !== null) return new Response(null, { status: allowed.has(asked.toLowerCase()) ? 204 : 403, headers: { "Cache-Control": "no-store" } });

    let target;
    try {
      target = new URL(url.searchParams.get("url") ?? "");
    } catch {
      return new Response("Give the address to relay as ?url=", { status: 400 });
    }
    if (!/^https?:$/.test(target.protocol) || !allowed.has(target.hostname.toLowerCase())) {
      return new Response(`${target.hostname} is not among the hosts this site relays (RELAY_HOSTS).`, { status: 403 });
    }

    const headers = new Headers();
    for (const name of forwarded) if (request.headers.has(name)) headers.set(name, request.headers.get(name));
    // A body is at most a book, so it is read whole rather than streamed.
    const hasBody = !["GET", "HEAD"].includes(request.method);
    const answer = await fetch(target, { method: request.method, headers, body: hasBody ? await request.arrayBuffer() : undefined });

    const kept = new Headers({ "Cache-Control": "no-store" });
    for (const name of returned) if (answer.headers.has(name)) kept.set(name, answer.headers.get(name));
    return new Response(answer.body, { status: answer.status, statusText: answer.statusText, headers: kept });
  },
};
