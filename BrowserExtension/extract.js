// Injected into the page: finds the text worth reading, strips everything
// else, and returns it as an XHTML fragment with its images' addresses.
// A selection, when there is a sizeable one, is taken as it is instead.
// The value of the last expression is what the popup receives.
(() => {
  const XHTML = "http://www.w3.org/1999/xhtml";
  const kept = new Set([
    "p", "h2", "h3", "h4", "h5", "h6", "blockquote", "ul", "ol", "li", "pre", "code",
    "em", "strong", "i", "b", "u", "s", "sub", "sup", "br", "hr", "figure", "figcaption",
    "table", "thead", "tbody", "tfoot", "tr", "th", "td", "caption", "dl", "dt", "dd",
    "q", "cite", "abbr", "small", "mark", "div",
  ]);
  const dropped = new Set([
    "script", "style", "noscript", "iframe", "form", "button", "input", "select", "textarea",
    "nav", "aside", "footer", "svg", "canvas", "video", "audio", "object", "embed", "template",
    "dialog", "menu", "link", "meta", "label",
  ]);
  const blocks = new Set(["div", "section", "article", "main", "header", "center"]);
  const noise = /(^|[\s_-])(comments?|share|sharing|social|promo|related|recommend\w*|newsletter|subscribe|advert\w*|ads?|sponsor\w*|sidebar|breadcrumbs?|cookies?|popup|modal|paywall|toolbar|tags|signup|widget)([\s_-]|$)/i;
  const invalid = /[\u0000-\u0008\u000B\u000C\u000E-\u001F\uFFFE\uFFFF]/g;

  const meta = (selector) => document.querySelector(selector)?.getAttribute("content")?.trim() || "";

  function isHidden(element) {
    if (element.hidden || element.getAttribute("aria-hidden") === "true") return true;
    const style = getComputedStyle(element);
    return style.display === "none" || style.visibility === "hidden";
  }

  function isNoise(element) {
    const label = `${element.id} ${typeof element.className === "string" ? element.className : ""} ${element.getAttribute("role") || ""}`;
    return noise.test(label) || /^(navigation|complementary|contentinfo|banner)$/.test(element.getAttribute("role") || "");
  }

  function linkDensity(element) {
    const text = element.textContent.length || 1;
    let links = 0;
    for (const link of element.querySelectorAll("a")) links += link.textContent.length;
    return links / text;
  }

  /** The element holding most of the page's prose. */
  function mainContent() {
    const scores = new Map();
    const add = (element, score) => element && scores.set(element, (scores.get(element) || 0) + score);
    for (const paragraph of document.body.querySelectorAll("p, pre, td, blockquote")) {
      const text = paragraph.textContent.trim();
      if (text.length < 25) continue;
      const score = 1 + text.split(/[,\u060C\uFF0C]/).length + Math.min(text.length / 100, 3);
      add(paragraph.parentElement, score);
      add(paragraph.parentElement?.parentElement, score / 2);
    }
    let best = null;
    let bestScore = 0;
    for (const [element, raw] of scores) {
      let score = raw * (1 - linkDensity(element));
      if (element.matches("article, main, [role=main], [itemprop=articleBody]")) score *= 1.5;
      if (isNoise(element)) score *= 0.3;
      if (score > bestScore) {
        best = element;
        bestScore = score;
      }
    }
    // An article's paragraphs often sit in several sibling blocks; the
    // article around them is the better choice when it has them all.
    const article = best?.closest("article, [itemprop=articleBody]");
    return article && article.textContent.length < best.textContent.length * 3 ? article : best || document.body;
  }

  function imageSource(image) {
    // Lazy loaders keep the real address aside and show a placeholder
    // until the image is scrolled to.
    const lazy = image.getAttribute("data-src") || image.getAttribute("data-lazy-src") || image.getAttribute("data-original");
    const current = image.currentSrc || image.getAttribute("src") || "";
    const source = lazy && (!current || current.startsWith("data:")) ? lazy : current;
    return source ? new URL(source, location.href).href : "";
  }

  function convert(node, target, images, inPre, isRoot) {
    if (node.nodeType === Node.TEXT_NODE) {
      let text = node.data.replace(invalid, "");
      if (!inPre) text = text.replace(/\s+/g, " ");
      if (text) target.appendChild(target.ownerDocument.createTextNode(text));
      return;
    }
    if (node.nodeType !== Node.ELEMENT_NODE && node.nodeType !== Node.DOCUMENT_FRAGMENT_NODE) return;
    const name = node.nodeType === Node.ELEMENT_NODE ? node.localName.toLowerCase() : "#fragment";
    if (dropped.has(name)) return;
    if (node.nodeType === Node.ELEMENT_NODE && !isRoot && node.isConnected && isHidden(node)) return;
    // A share bar or a related-links box is short; a long block that happens
    // to carry such a class is more likely the text itself.
    if (node.nodeType === Node.ELEMENT_NODE && !isRoot && isNoise(node) && node.textContent.length < 2000) return;
    // The chapter opens with the title already.
    if (name === "h1" && node.textContent.trim().replace(/\s+/g, " ") === title) return;

    if (name === "img") {
      const source = imageSource(node);
      const small = node.isConnected && node.naturalWidth > 0 && node.naturalWidth < 48 && node.naturalHeight < 48;
      if (!source || small) return;
      const image = target.ownerDocument.createElementNS(XHTML, "img");
      image.setAttribute("src", source);
      image.setAttribute("alt", (node.getAttribute("alt") || "").replace(invalid, ""));
      target.appendChild(image);
      images.add(source);
      return;
    }

    // The page's own title is repeated as the chapter's heading, so its h1s
    // step down a level.
    const tag = name === "h1" ? "h2" : blocks.has(name) ? "div" : name;
    let into = target;
    if (kept.has(tag)) {
      into = target.ownerDocument.createElementNS(XHTML, tag);
      for (const attribute of ["colspan", "rowspan"]) {
        if (node.hasAttribute?.(attribute)) into.setAttribute(attribute, node.getAttribute(attribute));
      }
      target.appendChild(into);
    }
    for (const child of node.childNodes) convert(child, into, images, inPre || tag === "pre", false);
    if (into !== target && tag !== "br" && tag !== "hr" && !into.textContent.trim() && !into.querySelector("img")) {
      target.removeChild(into);
    }
  }

  const selection = getSelection();
  const selected = selection && selection.rangeCount > 0 && selection.toString().trim().length > 200;
  const root = selected ? selection.getRangeAt(0).cloneContents() : mainContent();

  const heading = (root.querySelector?.("h1") || document.querySelector("h1"))?.textContent.trim();
  const title = (heading || meta('meta[property="og:title"]') || document.title.trim() || location.hostname).replace(/\s+/g, " ");

  const output = document.implementation.createDocument(XHTML, "html", null);
  const container = output.createElementNS(XHTML, "div");
  output.documentElement.appendChild(container);
  const images = new Set();
  convert(root, container, images, false, true);
  const byline = meta('meta[name="author"]') || document.querySelector('[rel="author"], [itemprop="author"]')?.textContent.trim() || "";
  const site = meta('meta[property="og:site_name"]') || location.hostname.replace(/^www\./, "");
  const language = (document.documentElement.lang || meta('meta[http-equiv="content-language"]') || meta('meta[property="og:locale"]'))
    .replace(/_/g, "-");

  return {
    title: title.replace(invalid, ""),
    byline: byline.replace(invalid, "").slice(0, 200),
    site,
    language,
    url: location.href,
    xhtml: new XMLSerializer().serializeToString(container),
    images: [...images],
  };
})();
