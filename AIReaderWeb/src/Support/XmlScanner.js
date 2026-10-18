// Walks XML (or the XHTML inside an EPUB) and reports tags and text to
// callbacks, so small parsers need no parser class each. Names come
// lowercased and without their namespace prefix; entities in text and
// attributes are decoded. Tolerant of the broken markup books carry, which
// a browser's XML parser would refuse. The same as the Kindle app's
// `Support/XmlScanner.cpp` and the iOS app's `Support/MarkupScanner.swift`,
// event for event.

// The HTML 4 entities in code-point order from U+00A0, so a name is found by
// its position; the ones after that are the typographic set books use.
const latin1Names = [
  "nbsp", "iexcl", "cent", "pound", "curren", "yen", "brvbar", "sect", "uml", "copy", "ordf", "laquo",
  "not", "shy", "reg", "macr", "deg", "plusmn", "sup2", "sup3", "acute", "micro", "para", "middot",
  "cedil", "sup1", "ordm", "raquo", "frac14", "frac12", "frac34", "iquest", "Agrave", "Aacute", "Acirc",
  "Atilde", "Auml", "Aring", "AElig", "Ccedil", "Egrave", "Eacute", "Ecirc", "Euml", "Igrave", "Iacute",
  "Icirc", "Iuml", "ETH", "Ntilde", "Ograve", "Oacute", "Ocirc", "Otilde", "Ouml", "times", "Oslash",
  "Ugrave", "Uacute", "Ucirc", "Uuml", "Yacute", "THORN", "szlig", "agrave", "aacute", "acirc", "atilde",
  "auml", "aring", "aelig", "ccedil", "egrave", "eacute", "ecirc", "euml", "igrave", "iacute", "icirc",
  "iuml", "eth", "ntilde", "ograve", "oacute", "ocirc", "otilde", "ouml", "divide", "oslash", "ugrave",
  "uacute", "ucirc", "uuml", "yacute", "thorn", "yuml",
];

const entities = new Map([
  ["amp", 0x26], ["lt", 0x3c], ["gt", 0x3e], ["quot", 0x22], ["apos", 0x27],
  ["OElig", 0x152], ["oelig", 0x153], ["Scaron", 0x160], ["scaron", 0x161], ["Yuml", 0x178],
  ["fnof", 0x192], ["circ", 0x2c6], ["tilde", 0x2dc], ["ensp", 0x20], ["emsp", 0x20], ["thinsp", 0x20],
  ["zwnj", 0], ["zwj", 0], ["lrm", 0], ["rlm", 0], ["ndash", 0x2013], ["mdash", 0x2014],
  ["lsquo", 0x2018], ["rsquo", 0x2019], ["sbquo", 0x201a], ["ldquo", 0x201c], ["rdquo", 0x201d],
  ["bdquo", 0x201e], ["dagger", 0x2020], ["Dagger", 0x2021], ["bull", 0x2022], ["hellip", 0x2026],
  ["permil", 0x2030], ["prime", 0x2032], ["Prime", 0x2033], ["lsaquo", 0x2039], ["rsaquo", 0x203a],
  ["oline", 0x203e], ["frasl", 0x2044], ["euro", 0x20ac], ["trade", 0x2122], ["larr", 0x2190],
  ["uarr", 0x2191], ["rarr", 0x2192], ["darr", 0x2193], ["harr", 0x2194], ["minus", 0x2212],
  ["infin", 0x221e], ["ne", 0x2260], ["le", 0x2264], ["ge", 0x2265], ["loz", 0x25ca],
]);
// The soft hyphen is invisible and best dropped.
latin1Names.forEach((name, index) => entities.set(name, name === "shy" ? 0 : 0xa0 + index));

/**
 * What `&…;` stands for: the character of a known name, or of a number that
 * is a Unicode scalar value other than 0; "" for any other number; the text
 * as written for an unknown name. Only ASCII letters, digits and '#' make a
 * reference, so the apps count it alike.
 */
function reference(whole, entity) {
  if (entity[0] !== "#") {
    const code = entities.get(entity);
    if (code === undefined) return whole;
    return code ? String.fromCodePoint(code) : "";
  }
  const hex = entity[1] === "x" || entity[1] === "X";
  const digits = entity.slice(hex ? 2 : 1);
  if (!(hex ? /^[0-9A-Fa-f]+$/ : /^[0-9]+$/).test(digits)) return "";
  const code = parseInt(digits, hex ? 16 : 10);
  if (code === 0 || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return "";
  return String.fromCodePoint(code);
}

export function decodeEntities(text) {
  if (!text.includes("&")) return text;
  return text.replace(/&([A-Za-z0-9#]{1,9});/g, reference);
}

/** Lowercased by ASCII alone, as markup names are, whatever the locale. */
function lowerAscii(name) {
  return name.replace(/[A-Z]+/g, (letters) => letters.toLowerCase());
}

function localName(name) {
  const colon = name.lastIndexOf(":");
  return lowerAscii(colon < 0 ? name : name.slice(colon + 1));
}

function isNameChar(c) {
  return !(c === " " || c === "\t" || c === "\r" || c === "\n" || c === "/" || c === ">" || c === "=");
}

function isWhite(c) {
  return c === " " || c === "\t" || c === "\r" || c === "\n";
}

/** The `name="value"` pairs of a tag body. */
function attributes(body) {
  const result = {};
  let i = 0;
  while (i < body.length) {
    while (i < body.length && !isNameChar(body[i])) i++;
    const nameStart = i;
    while (i < body.length && isNameChar(body[i])) i++;
    if (i === nameStart) break;
    const name = localName(body.slice(nameStart, i));
    while (i < body.length && isWhite(body[i])) i++;
    if (i >= body.length || body[i] !== "=") {
      result[name] = "";
      continue;
    }
    i++;
    while (i < body.length && isWhite(body[i])) i++;
    if (i >= body.length) break;
    let value;
    if (body[i] === '"' || body[i] === "'") {
      const quote = body[i++];
      let end = body.indexOf(quote, i);
      if (end < 0) end = body.length;
      value = body.slice(i, end);
      i = end + 1;
    } else {
      const start = i;
      while (i < body.length && isNameChar(body[i])) i++;
      value = body.slice(start, i);
    }
    result[name] = decodeEntities(value);
  }
  return result;
}

/** A '<' opens a tag only before a letter, '/', '!' or '?'; any other is text, as a browser keeps "1 < 2". */
function opensTag(c) {
  return (c >= "a" && c <= "z") || (c >= "A" && c <= "Z") || c === "/" || c === "!" || c === "?";
}

/** Calls `onStart(name, attributes)`, `onEnd(name)` and `onText(text)` as they come. */
export function scan(markup, { onStart, onEnd, onText } = {}) {
  const n = markup.length;
  let i = 0;
  const emitText = (start, end, decode) => {
    if (end <= start || !onText) return;
    const text = markup.slice(start, end);
    onText(decode ? decodeEntities(text) : text);
  };

  while (i < n) {
    let open = markup.indexOf("<", i);
    while (open >= 0 && !opensTag(markup[open + 1])) open = markup.indexOf("<", open + 1);
    if (open < 0) {
      emitText(i, n, true);
      break;
    }
    emitText(i, open, true);
    i = open;

    if (markup.startsWith("<!--", i)) {
      const end = markup.indexOf("-->", i + 4);
      i = end < 0 ? n : end + 3;
      continue;
    }
    if (markup.startsWith("<![CDATA[", i)) {
      const end = markup.indexOf("]]>", i + 9);
      emitText(i + 9, end < 0 ? n : end, false);
      i = end < 0 ? n : end + 3;
      continue;
    }
    if (markup.startsWith("<?", i) || markup.startsWith("<!", i)) {
      const end = markup.indexOf(">", i);
      i = end < 0 ? n : end + 1;
      continue;
    }

    // A tag. Quoted attribute values may contain '>', so skip over them.
    let end = i + 1;
    let quote = "";
    while (end < n) {
      const c = markup[end];
      if (quote) {
        if (c === quote) quote = "";
      } else if (c === '"' || c === "'") {
        quote = c;
      } else if (c === ">") {
        break;
      }
      end++;
    }
    let body = markup.slice(i + 1, end);
    i = end < n ? end + 1 : n;
    if (!body) continue;

    // The closing slash goes first, so "</>" leaves nothing to look at.
    const closing = body[0] === "/";
    if (closing) body = body.slice(1);
    const selfClosing = body.endsWith("/");
    if (selfClosing) body = body.slice(0, -1);

    let nameEnd = 0;
    while (nameEnd < body.length && isNameChar(body[nameEnd])) nameEnd++;
    const name = localName(body.slice(0, nameEnd));
    if (!name) continue;

    if (closing) {
      onEnd?.(name);
      continue;
    }
    onStart?.(name, attributes(body.slice(nameEnd)));
    if (selfClosing) onEnd?.(name);
  }
}
