// The WebDAV folder AIReader syncs through. Books go into its `Books`
// folder, where both apps fetch them on their next sync.

export const booksFolder = "Books";

/** The folder URL, with a trailing slash. */
function folderURL(settings, ...names) {
  const base = settings.url.trim().replace(/\/+$/, "");
  return [base, ...names.map(encodeURIComponent)].join("/") + "/";
}

async function send(method, url, settings, { body, headers = {} } = {}) {
  const credentials = `${settings.username}:${settings.password}`;
  const bytes = new TextEncoder().encode(credentials);
  const authorization = settings.username || settings.password
    ? { Authorization: `Basic ${btoa(String.fromCharCode(...bytes))}` }
    : {};
  const response = await fetch(url, { method, body, headers: { ...authorization, ...headers }, credentials: "omit" });
  if (response.status === 401 || response.status === 403) {
    throw new Error("The server refused the user name or password.");
  }
  return response;
}

function check(response) {
  if (!response.ok) throw new Error(`The server answered ${response.status}.`);
}

/** The names of the files in the books folder; none if it is not there yet. */
export async function listBooks(settings) {
  const response = await send("PROPFIND", folderURL(settings, booksFolder), settings, { headers: { Depth: "1" } });
  if (response.status === 404) return [];
  check(response);
  const xml = new DOMParser().parseFromString(await response.text(), "application/xml");
  const names = [];
  for (const entry of xml.getElementsByTagNameNS("DAV:", "response")) {
    if (entry.getElementsByTagNameNS("DAV:", "collection").length > 0) continue;
    const href = entry.getElementsByTagNameNS("DAV:", "href")[0]?.textContent.trim() ?? "";
    const name = decodeURIComponent(href.replace(/\/+$/, "").split("/").pop());
    if (name) names.push(name);
  }
  return names;
}

/**
 * Makes the folder, and the folders above it that are missing too: a
 * server makes only one level at a time, answering 409 when the one above
 * is not there.
 */
async function makeFolder(url, settings, depth = 0) {
  const response = await send("MKCOL", url, settings);
  if (response.status !== 409 || depth > 8) return;
  await makeFolder(new URL("..", url).href, settings, depth + 1);
  await send("MKCOL", url, settings);
}

/** Stores a book in the books folder under `name`. */
export async function putBook(name, blob, settings) {
  const folder = folderURL(settings, booksFolder);
  const url = folder + encodeURIComponent(name);
  const options = { body: blob, headers: { "Content-Type": "application/epub+zip" } };
  let response = await send("PUT", url, settings, options);
  if (response.status === 409 || response.status === 404) {
    await makeFolder(folder, settings);
    response = await send("PUT", url, settings, options);
  }
  check(response);
}

/** Whether the folder is there; throws when the server cannot be reached or refuses. */
export async function checkFolder(settings) {
  const response = await send("PROPFIND", folderURL(settings), settings, { headers: { Depth: "0" } });
  if (response.status === 404) return false;
  check(response);
  return true;
}
