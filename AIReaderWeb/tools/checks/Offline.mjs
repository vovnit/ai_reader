// The service worker stores the app for offline use from a list of its
// files; one missing from the list would be missing offline.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { check } from "./Check.mjs";

const root = fileURLToPath(new URL("../../", import.meta.url));

function files(folder) {
  return readdirSync(root + folder).flatMap((name) => {
    const path = `${folder}/${name}`;
    return statSync(root + path).isDirectory() ? files(path) : [path];
  });
}

export function checkOffline() {
  const worker = readFileSync(`${root}sw.js`, "utf8");
  const listed = new Set([...worker.matchAll(/"\.\/([^"]*)"/g)].map((match) => match[1]));
  // index.html is listed as "./", the page itself.
  const wanted = ["", "app.css", "manifest.webmanifest", "icon.svg", "icon-192.png", "icon-512.png", ...files("src")];
  const missing = wanted.filter((file) => !listed.has(file));
  const stale = [...listed].filter((file) => file && !existsSync(root + file)).map((file) => `no such file ${file}`);
  if (listed.has("index.html")) stale.push("index.html listed: keep the page as ./ only");
  check("the service worker lists every file of the app", !missing.length && !stale.length, [...missing.map((file) => `missing ${file}`), ...stale].join(", "));
}
