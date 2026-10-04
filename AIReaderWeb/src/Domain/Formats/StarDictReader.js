// StarDict: an `.ifo` describing the dictionary, an `.idx` listing every
// headword with where its article sits, and a `.dict` holding the articles.
import { languageNameCode } from "./DictionaryFormat.js";

const decoder = new TextDecoder();

/** The `key=value` lines of an `.ifo`. */
function settings(text) {
  const result = {};
  for (const line of text.split("\n")) {
    const equals = line.indexOf("=");
    if (equals >= 0) result[line.slice(0, equals).trim().toLowerCase()] = line.slice(equals + 1).trim();
  }
  return result;
}

/** Enough of an HTML stripper for dictionary articles; tags that break a line keep breaking it. */
export function stripTags(text) {
  let output = "";
  let depth = 0;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === "<") {
      const tag = text.slice(i + 1, i + 5).toLowerCase();
      if (/^(br|p>|p |\/p>|div|li)/.test(tag)) output += "\n";
      depth++;
    } else if (c === ">") {
      if (depth) depth--;
    } else if (!depth) {
      output += c;
    }
  }
  return output.replaceAll("&nbsp;", " ").replaceAll("&lt;", "<").replaceAll("&gt;", ">").replaceAll("&quot;", '"').replaceAll("&amp;", "&");
}

/** One field, unwrapped of the markup its type implies. */
function fieldText(bytes, type) {
  if ("hxg".includes(type)) return stripTags(decoder.decode(bytes));
  if ("mltykw".includes(type)) return decoder.decode(bytes);
  return "";
}

/**
 * An article's senses. With `sametypesequence` the block is one field of a
 * known type; without it each field announces its own type first.
 */
function senses(article, sameType) {
  const texts = [];
  if (sameType.length === 1) {
    texts.push(fieldText(article, sameType));
  } else {
    let cursor = 0;
    while (cursor < article.length) {
      const type = String.fromCharCode(article[cursor++]);
      if (type >= "A" && type <= "Z") {
        // A length-prefixed, and so binary, field.
        const size = new DataView(article.buffer, article.byteOffset).getUint32(cursor);
        cursor = Math.min(article.length, cursor + 4 + size);
      } else {
        let end = article.indexOf(0, cursor);
        if (end < 0) end = article.length;
        texts.push(fieldText(article.subarray(cursor, end), type));
        cursor = end < article.length ? end + 1 : end;
      }
    }
  }
  return texts.flatMap((block) => block.split("\n")).map((line) => line.trim()).filter(Boolean);
}

/** `ifo` is the `.ifo`'s text; `index` and `body` the `.idx` and `.dict` bytes, ungzipped. */
export function readStarDict(ifo, index, body, fallbackName, entry) {
  const info = settings(ifo);
  const wide = info.idxoffsetbits === "64";
  const sameType = info.sametypesequence ?? "";
  const view = new DataView(index.buffer, index.byteOffset, index.byteLength);
  let cursor = 0;
  while (cursor < index.length) {
    const end = index.indexOf(0, cursor);
    if (end < 0) break;
    const word = decoder.decode(index.subarray(cursor, end));
    cursor = end + 1;
    if (cursor + (wide ? 12 : 8) > index.length) break;
    const offset = wide ? Number(view.getBigUint64(cursor)) : view.getUint32(cursor);
    cursor += wide ? 8 : 4;
    const size = view.getUint32(cursor);
    cursor += 4;
    if (size === 0 || offset + size > body.length) continue;
    const found = senses(body.subarray(offset, offset + size), sameType);
    if (word && found.length) entry({ headword: word, partOfSpeech: "", senses: found });
  }
  return {
    name: info.bookname ?? fallbackName,
    targetLanguage: languageNameCode(info.lang ?? info.sourcelang),
    definitionLanguage: languageNameCode(info.targetlang),
  };
}
