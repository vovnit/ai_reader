// The table of contents out of an EPUB's navigation document: the NCX of
// EPUB 2, or the `<nav epub:type="toc">` of EPUB 3. Entries are
// `{ title, href, depth }`, the href as written.
import { scan } from "../../Support/XmlScanner.js";

/** Entries with a title and somewhere to go; titles on one line. */
function tidy(entries) {
  return entries
    .map((entry) => ({ ...entry, title: entry.title.trim() }))
    .filter((entry) => entry.title && entry.href);
}

export function fromNcx(xml) {
  const entries = [];
  // The navPoints open at this point, so a label lands on the innermost one.
  const open = [];
  let inLabel = false;
  scan(xml, {
    onStart(name, attributes) {
      if (name === "navpoint") {
        entries.push({ title: "", href: "", depth: open.length });
        open.push(entries.length - 1);
      } else if (name === "navlabel") {
        inLabel = open.length > 0;
      } else if (name === "content" && open.length) {
        entries[open.at(-1)].href = attributes.src ?? "";
      }
    },
    onText(text) {
      if (inLabel) entries[open.at(-1)].title += text;
    },
    onEnd(name) {
      if (name === "navpoint") open.pop();
      else if (name === "navlabel") inLabel = false;
    },
  });
  return tidy(entries);
}

export function fromNav(xhtml) {
  // Each <nav> is read on its own; the one typed "toc" is the table of
  // contents, and failing that the first one is taken.
  const navs = [];
  const isToc = [];
  let listDepth = 0;
  let inLink = false;
  scan(xhtml, {
    onStart(name, attributes) {
      if (name === "nav") {
        navs.push([]);
        isToc.push((attributes.type ?? "").includes("toc"));
        listDepth = 0;
        return;
      }
      if (!navs.length) return;
      if (name === "ol" || name === "ul") listDepth++;
      else if (name === "li") navs.at(-1).push({ title: "", href: "", depth: Math.max(listDepth - 1, 0) });
      else if (name === "a" && navs.at(-1).length) {
        navs.at(-1).at(-1).href = attributes.href ?? "";
        inLink = true;
      }
    },
    onText(text) {
      if (inLink) navs.at(-1).at(-1).title += text;
    },
    onEnd(name) {
      if (!navs.length) return;
      if (name === "a") inLink = false;
      else if ((name === "ol" || name === "ul") && listDepth) listDepth--;
    },
  });
  const toc = navs.findIndex((_, index) => isToc[index]);
  return tidy(navs[toc >= 0 ? toc : 0] ?? []);
}

/** Whichever the document is; an NCX has an `<ncx>` root. */
export function parseNavigation(markup) {
  let ncx = false;
  scan(markup, {
    onStart(name) {
      if (name === "ncx") ncx = true;
    },
  });
  return ncx ? fromNcx(markup) : fromNav(markup);
}

/**
 * The entry of `contents` the reader is in: the one nearest before a
 * place, or -1 before the first. Entries are read as the book lists them,
 * which is nearly always in order but need not be.
 */
export function contentsEntryAt(contents, chapter, offset) {
  let found = -1;
  contents.forEach((entry, index) => {
    if (entry.chapter > chapter || (entry.chapter === chapter && entry.offset > offset)) return;
    const best = contents[found];
    if (found < 0 || entry.chapter > best.chapter || (entry.chapter === best.chapter && entry.offset >= best.offset)) {
      found = index;
    }
  });
  return found;
}
