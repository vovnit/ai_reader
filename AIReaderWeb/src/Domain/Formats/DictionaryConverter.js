// Reads a dictionary in any of the formats readers hand around, handing
// each article to `entry({ headword, partOfSpeech, senses })`. Returns
// `{ name, targetLanguage, definitionLanguage }`; throws with a message fit
// to show.
import { contents, lines, text } from "../../Support/TextFile.js";
import { readDelimited } from "./DelimitedDictionaryReader.js";
import { companion, defaultName } from "./DictionaryFormat.js";
import { readDsl } from "./DSLDictionaryReader.js";
import { readStarDict } from "./StarDictReader.js";
import { readXdxf } from "./XDXFDictionaryReader.js";

export async function convertDictionary(source, entry) {
  const fallback = defaultName(source);
  switch (source.format) {
    case "delimited":
      readDelimited(await text(source.main), entry);
      return { name: fallback, targetLanguage: "", definitionLanguage: "" };
    case "xdxf":
      return readXdxf(await text(source.main), fallback, entry);
    case "dsl":
      return readDsl(await lines(source.main), fallback, entry);
    case "stardict": {
      const index = companion(source, ".idx");
      const body = companion(source, ".dict");
      if (!index || !body) throw new Error("A StarDict dictionary needs its .idx and .dict files picked with the .ifo.");
      try {
        return readStarDict(await text(source.main), await contents(index), await contents(body), fallback, entry);
      } catch {
        throw new Error(`${source.main.name} could not be read.`);
      }
    }
    default:
      throw new Error(`${source.main.name} is not a dictionary format this app converts.`);
  }
}
