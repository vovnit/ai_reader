// XDXF, the open XML dictionary format. Articles are `<ar>` elements
// holding one or more `<k>` headwords and the definition text around them.
import { scan } from "../../Support/XmlScanner.js";
import { languageNameCode } from "./DictionaryFormat.js";

/** Returns `{ name, targetLanguage, definitionLanguage }` and hands each article to `entry`. */
export function readXdxf(markup, fallbackName, entry) {
  const info = { name: fallbackName, targetLanguage: "", definitionLanguage: "" };
  let headwords = [];
  let body = "";
  let text = "";
  let inArticle = false;
  let inKey = false;
  let inName = false;
  scan(markup, {
    onStart(name, attributes) {
      if (name === "xdxf") {
        info.targetLanguage = languageNameCode(attributes.lang_from);
        info.definitionLanguage = languageNameCode(attributes.lang_to);
      } else if (name === "ar") {
        inArticle = true;
        headwords = [];
        body = "";
      } else if (name === "k" && inArticle) {
        inKey = true;
        text = "";
      } else if (name === "full_name") {
        inName = true;
        text = "";
      }
    },
    onText(chunk) {
      if (inKey || inName) text += chunk;
      else if (inArticle) body += chunk;
    },
    onEnd(name) {
      if (name === "full_name") {
        inName = false;
        if (text.trim()) info.name = text.trim();
      } else if (name === "k") {
        inKey = false;
        if (text.trim()) headwords.push(text.trim());
      } else if (name === "ar") {
        inArticle = false;
        const senses = body.split("\n").map((line) => line.trim()).filter(Boolean);
        if (!senses.length) return;
        for (const headword of headwords) entry({ headword, partOfSpeech: "", senses });
      }
    },
  });
  return info;
}
