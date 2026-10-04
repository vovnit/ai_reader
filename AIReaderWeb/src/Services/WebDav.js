// What sync needs of a WebDAV server: fetch one file, store one file, and
// list a folder. Any server that speaks GET, PUT and PROPFIND with a
// password will do — and, for a page, answers requests from it (CORS).
// Agrees with `WebDAV.swift`, `WebDav.cpp` and the extension's
// `lib/webdav.js` on making folders and escaping names.
import { scan } from "../Support/XmlScanner.js";
import { fetchResponse } from "./Http.js";

function authorization(settings) {
  if (!settings.username && !settings.password) return {};
  const bytes = new TextEncoder().encode(`${settings.username}:${settings.password}`);
  return { Authorization: `Basic ${btoa(String.fromCharCode(...bytes))}` };
}

async function request(method, url, settings, { body, headers = {} } = {}) {
  // A book can take minutes on a slow line.
  return fetchResponse(url, { method, body, headers: { ...authorization(settings), ...headers }, seconds: 600 });
}

function check(status) {
  if (status === 401 || status === 403) throw new Error("The server refused the user name or password.");
  if (status < 200 || status >= 300) throw new Error(`The server answered ${status}.`);
}

/** The URL with its last part taken off. */
function folderOf(url) {
  const slash = url.lastIndexOf("/");
  return slash < 0 ? url : url.slice(0, slash);
}

/**
 * Makes the folder, and the folders above it that are missing too: a
 * server makes only one level at a time, answering 409 when the one above
 * is not there.
 */
async function makeFolder(url, settings, depth = 0) {
  if ((await request("MKCOL", url, settings)).status !== 409 || depth > 8) return;
  await makeFolder(folderOf(url), settings, depth + 1);
  await request("MKCOL", url, settings);
}

/** The file's bytes, or null when there is no such file yet. */
export async function download(url, settings) {
  const response = await request("GET", url, settings);
  if (response.status === 404) return null;
  check(response.status);
  return new Uint8Array(await response.arrayBuffer());
}

/** Stores the file, making its folder first if the server says there is none. */
export async function upload(url, contents, settings, type = "application/json") {
  const options = { body: contents, headers: { "Content-Type": type } };
  let response = await request("PUT", url, settings, options);
  if (response.status === 409 || response.status === 404) {
    await makeFolder(folderOf(url), settings);
    response = await request("PUT", url, settings, options);
  }
  check(response.status);
}

/** The names of the files in a folder, folders left out; none when there is no such folder yet. */
export async function list(folderUrl, settings) {
  const response = await request("PROPFIND", folderUrl, settings, { headers: { Depth: "1" } });
  if (response.status === 404) return [];
  check(response.status);
  return fileNames(await response.text());
}

function decode(text) {
  try {
    return decodeURIComponent(text);
  } catch {
    return text;
  }
}

/** The file names a PROPFIND answer lists, decoded; folders are left out. */
export function fileNames(multistatus) {
  const names = [];
  let href = "";
  let isFolder = false;
  let element = "";
  const close = () => {
    const trimmed = href.replace(/\/+$/, "");
    const name = decode(trimmed.slice(trimmed.lastIndexOf("/") + 1));
    if (!isFolder && name) names.push(name);
    href = "";
    isFolder = false;
  };
  scan(multistatus, {
    onStart(name) {
      element = name;
      if (name === "response" && href) close();
      if (name === "collection") isFolder = true;
    },
    onEnd() {
      element = "";
    },
    onText(text) {
      if (element === "href") href += text;
    },
  });
  if (href) close();
  return names;
}

/** A name made safe to put in a URL's path: everything but letters, digits and `-._~` percent-encoded. */
export function escapeName(name) {
  return [...new TextEncoder().encode(name)]
    .map((byte) => (/[A-Za-z0-9\-._~]/.test(String.fromCharCode(byte)) && byte < 0x80 ? String.fromCharCode(byte) : `%${byte.toString(16).toUpperCase().padStart(2, "0")}`))
    .join("");
}
