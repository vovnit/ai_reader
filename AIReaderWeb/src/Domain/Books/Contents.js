// The table of contents the book gives, pointed into the chapters kept:
// `[{ title, chapter, offset, depth }]`. An entry for a file dropped as
// blank, or never in the spine, is left out; a book without one gets an
// entry per chapter, named by its first heading.
import { directoryOf, navigationItem, resolve, unescapeHref } from "./EpubPackage.js";
import { parseNavigation } from "./EpubNavigation.js";

function chapterName(chapter, index) {
  for (const span of chapter.spans) {
    if (span.kind !== "heading") continue;
    const heading = chapter.text.slice(span.start, span.end).split("\n")[0].trim();
    if (heading) return heading;
  }
  return `Chapter ${index + 1}`;
}

export async function contentsEntries(archive, pkg, directory, chapterByEntry, chapters) {
  const entries = [];
  const item = navigationItem(pkg);
  const navPath = item ? resolve(item.href, directory) : "";
  const markup = navPath ? await archive.text(navPath) : null;
  for (const nav of markup ? parseNavigation(markup) : []) {
    const hash = nav.href.indexOf("#");
    const file = unescapeHref(hash < 0 ? nav.href : nav.href.slice(0, hash));
    const fragment = hash < 0 ? "" : nav.href.slice(hash + 1);
    const chapter = chapterByEntry.get(resolve(file, directoryOf(navPath)));
    if (chapter === undefined) continue;
    entries.push({ title: nav.title, chapter, offset: chapters[chapter].anchors.get(fragment) ?? 0, depth: nav.depth });
  }
  if (entries.length) return entries;
  return chapters.map((chapter, index) => ({ title: chapterName(chapter, index), chapter: index, offset: 0, depth: 0 }));
}
