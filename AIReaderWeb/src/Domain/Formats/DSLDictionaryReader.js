// ABBYY Lingvo's DSL. Headwords start at column zero; the lines indented
// under them are the article, marked up with `[tags]` this reader strips.
import { languageNameCode } from "./DictionaryFormat.js";

/** `#NAME "Big Dictionary"` split into its parts. */
function directive(line) {
  if (!line.startsWith("#")) return null;
  const space = line.indexOf(" ");
  const key = (space < 0 ? line : line.slice(0, space)).toLowerCase();
  const value = space < 0 ? "" : line.slice(space + 1).replace(/^[ "]+/, "").replace(/[ "\r]+$/, "");
  return [key, value];
}

/** Removes DSL markup, keeping the words inside it. */
export function stripDsl(line) {
  let output = "";
  let depth = 0;
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    // A backslash escapes the character after it.
    if (c === "\\" && i + 1 < line.length) {
      output += line[++i];
      continue;
    }
    // `{{...}}` are comments.
    if (c === "{" && line[i + 1] === "{") {
      const end = line.indexOf("}}", i + 2);
      if (end >= 0) {
        i = end + 1;
        continue;
      }
    }
    if (c === "[") depth++;
    else if (c === "]") depth = Math.max(depth - 1, 0);
    // `{}` mark headword parts that are shown but not indexed.
    else if (c !== "{" && c !== "}" && c !== "\r" && depth === 0) output += c;
  }
  return output;
}

/** Returns `{ name, targetLanguage, definitionLanguage }` and hands each article to `entry`. */
export function readDsl(lines, fallbackName, entry) {
  const info = { name: fallbackName, targetLanguage: "", definitionLanguage: "" };
  let headwords = [];
  let senses = [];
  const flush = () => {
    if (headwords.length && senses.length) {
      for (const headword of headwords) entry({ headword, partOfSpeech: "", senses });
    }
    headwords = [];
    senses = [];
  };
  for (const line of lines) {
    const found = directive(line);
    if (found) {
      const [key, value] = found;
      if (key === "#name") info.name = value;
      else if (key === "#index_language") info.targetLanguage = languageNameCode(value);
      else if (key === "#contents_language") info.definitionLanguage = languageNameCode(value);
      continue;
    }
    const indented = line[0] === "\t" || line[0] === " ";
    const text = stripDsl(line).trim();
    if (indented) {
      if (text) senses.push(text);
    } else {
      // A run of headwords shares the article indented below it.
      if (senses.length) flush();
      if (text) headwords.push(text);
    }
  }
  flush();
  return info;
}
