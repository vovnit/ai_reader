// The EPUB package document (the `.opf` file): what the book is called,
// which files it is made of, and in what order they are read.
import { scan } from "../../Support/XmlScanner.js";

export function unescapeHref(href) {
  try {
    return decodeURIComponent(href);
  } catch {
    return href;
  }
}

/** `{ title, author, language, items: Map(id → item), spine: [id], coverItemId, ncxItemId }` */
export function parsePackage(xml) {
  const result = { title: "", author: "", language: "", items: new Map(), spine: [], coverItemId: "", ncxItemId: "" };
  let inSpine = false;
  let open = "";
  let text = "";
  scan(xml, {
    onStart(name, attributes) {
      open = name;
      text = "";
      if (name === "item") {
        const { id = "", href = "" } = attributes;
        if (!id || !href) return;
        result.items.set(id, {
          id,
          href: unescapeHref(href),
          mediaType: attributes["media-type"] ?? "",
          properties: attributes.properties ?? "",
        });
      } else if (name === "spine") {
        inSpine = true;
        result.ncxItemId = attributes.toc ?? "";
      } else if (name === "itemref") {
        if (inSpine && attributes.idref) result.spine.push(attributes.idref);
      } else if (name === "meta" && attributes.name === "cover") {
        result.coverItemId = attributes.content ?? "";
      }
    },
    onText(chunk) {
      text += chunk;
    },
    onEnd(name) {
      const value = text.trim();
      if (name === open && value) {
        if (name === "title" && !result.title) result.title = value;
        else if (name === "creator" && !result.author) result.author = value;
        else if (name === "language" && !result.language) result.language = value;
      }
      text = "";
    },
  });
  return result;
}

/** Items to render, in reading order, limited to markup documents. */
export function readingOrder(pkg) {
  return pkg.spine.map((id) => pkg.items.get(id)).filter((item) => item && item.mediaType.includes("html"));
}

/** The item holding the cover image: flagged `cover-image` (EPUB 3) or named by the `cover` meta (EPUB 2). */
export function coverItem(pkg) {
  for (const item of pkg.items.values()) if (item.properties.includes("cover-image")) return item;
  return pkg.items.get(pkg.coverItemId) ?? null;
}

/** The item holding the table of contents: the EPUB 3 navigation document, else the NCX. */
export function navigationItem(pkg) {
  for (const item of pkg.items.values()) if (item.properties.split(" ").includes("nav")) return item;
  if (pkg.items.has(pkg.ncxItemId)) return pkg.items.get(pkg.ncxItemId);
  for (const item of pkg.items.values()) if (item.mediaType === "application/x-dtbncx+xml") return item;
  return null;
}

/** Reads `META-INF/container.xml` for the package document's path. */
export function packagePath(containerXml) {
  let path = "";
  scan(containerXml, {
    onStart(name, attributes) {
      if (name === "rootfile" && !path && attributes["full-path"]) path = attributes["full-path"];
    },
  });
  return path;
}

/** An href relative to `directory`, as a path from the archive root. */
export function resolve(href, directory) {
  const combined = directory ? `${directory}/${href}` : href;
  const components = [];
  for (const part of combined.split("/")) {
    if (!part || part === ".") continue;
    if (part === "..") components.pop();
    else components.push(part);
  }
  return components.join("/");
}

export function directoryOf(path) {
  const slash = path.lastIndexOf("/");
  return slash < 0 ? "" : path.slice(0, slash);
}
