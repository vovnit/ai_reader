// Reads a folder of photographed pages as the web app does — Tesseract.js
// in a browser, the finger, the word it points at — and, given a git
// commit, reads them again as that commit did and says where the two
// differ. A photo reader's change is checked on real photos this way.
//
//   node tools/photos.mjs <folder> [commit]
//
// Needs Playwright and its Chromium (`npm install --no-save playwright`
// and `npx playwright install chromium` in this folder), and the network
// the first time a language's model is needed.
import { execFileSync } from "node:child_process";
import { mkdtempSync, readdirSync, readFileSync, rmSync, statSync } from "node:fs";
import { createServer } from "node:http";
import { createRequire } from "node:module";
import { tmpdir } from "node:os";
import { extname, join, resolve } from "node:path";

const [folder, commit] = process.argv.slice(2);
if (!folder) {
  console.error("usage: node tools/photos.mjs <folder> [commit]");
  process.exit(1);
}
let chromium;
try {
  // `require` also finds a copy installed for every project (NODE_PATH).
  ({ chromium } = createRequire(import.meta.url)("playwright"));
} catch {
  console.error("Playwright is missing: npm install --no-save playwright && npx playwright install chromium");
  process.exit(1);
}

const app = resolve(import.meta.dirname, "..");
const photos = readdirSync(folder).filter((name) => /\.(jpe?g|png|heic|webp)$/i.test(name)).sort();
const types = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript", ".json": "application/json",
  ".wasm": "application/wasm", ".svg": "image/svg+xml", ".png": "image/png", ".jpg": "image/jpeg", ".jpeg": "image/jpeg",
  ".webp": "image/webp", ".heic": "image/heic" };

/** Serves `root` and the photos under /photos/, on a free port. */
function serve(root) {
  const server = createServer((request, response) => {
    const path = decodeURIComponent(new URL(request.url, "http://localhost").pathname);
    const file = path.startsWith("/photos/") ? join(folder, path.slice(8)) : join(root, path);
    try {
      if (!statSync(file).isFile()) throw new Error();
      response.writeHead(200, { "content-type": types[extname(file).toLowerCase()] ?? "application/octet-stream" });
      response.end(readFileSync(file));
    } catch {
      response.writeHead(404).end();
    }
  });
  return new Promise((done) => server.listen(0, "127.0.0.1", () => done(server)));
}

/** What the app at `root` makes of each photo: the pointed word, and the text read. */
async function readAll(browser, root) {
  const server = await serve(root);
  const page = await browser.newPage();
  await page.goto(`http://127.0.0.1:${server.address().port}/icon.svg`);
  const results = {};
  for (const name of photos) {
    results[name] = await page.evaluate(async (name) => {
      const { PhotoReader, canRead } = await import("/src/Services/PhotoReader.js");
      const { photoPassage } = await import("/src/Domain/Photo/PhotoPassage.js");
      const { detectLanguage } = await import("/src/Domain/Books/LanguageDetector.js");
      globalThis.reader ??= new PhotoReader();
      const file = await (await fetch(`/photos/${encodeURIComponent(name)}`)).blob();
      try {
        // As the app does: read again in the language the text turns out to be in.
        let passage = photoPassage(await globalThis.reader.read(file, globalThis.language ?? "en"));
        const found = detectLanguage([{ text: passage.text }]);
        if (found && found !== (globalThis.language ?? "en") && canRead(found)) {
          globalThis.language = found;
          passage = photoPassage(await globalThis.reader.read(file, found));
        }
        const word = passage.pointedOffset === null ? null : passage.text.slice(passage.pointedOffset).split(/\s/)[0];
        return { word, text: passage.text };
      } catch (error) {
        return { word: null, text: "", error: error.message };
      }
    }, name);
    process.stderr.write(".");
  }
  process.stderr.write("\n");
  await page.close();
  server.close();
  return results;
}

const browser = await chromium.launch();
let before = null;
let checkout = null;
try {
  if (commit) {
    checkout = mkdtempSync(join(tmpdir(), "aireader-photos-"));
    execFileSync("git", ["-C", app, "worktree", "add", "--detach", checkout, commit], { stdio: "ignore" });
    before = await readAll(browser, join(checkout, "AIReaderWeb"));
  }
  const now = await readAll(browser, app);
  let pointed = 0, pointedBefore = 0, changed = 0;
  for (const name of photos) {
    const result = now[name];
    if (result.word) pointed++;
    const old = before?.[name];
    if (old?.word) pointedBefore++;
    const differs = old && old.word !== result.word;
    if (differs) changed++;
    const words = (text) => (text ? text.split(/\s+/).length : 0);
    console.log([differs ? "≠" : " ", name, old ? `${old.word ?? "—"} → ${result.word ?? "—"}` : result.word ?? "—",
      old ? `(${words(old.text)} → ${words(result.text)} words)` : `(${words(result.text)} words)`, result.error ?? ""].join("  "));
  }
  console.log(before
    ? `${photos.length} photos: a word pointed at in ${pointedBefore} before, ${pointed} now; ${changed} differ`
    : `${photos.length} photos: a word pointed at in ${pointed}`);
} finally {
  await browser.close();
  if (checkout) {
    execFileSync("git", ["-C", app, "worktree", "remove", "--force", checkout], { stdio: "ignore" });
    rmSync(checkout, { recursive: true, force: true });
  }
}
