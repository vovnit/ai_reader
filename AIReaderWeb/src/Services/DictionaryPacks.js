// The dictionaries the app searches: the bundled one, plus any the reader
// adds. A pack this app understands is kept as it is; the other formats are
// converted on the way in, which for a large dictionary takes a while. A
// pack is `{ id, name, kind, fileName, targetLanguage, definitionLanguage,
// isEnabled, addedAt }`, its kind `bundled`, `file` or `converted`.
import { convertDictionary } from "../Domain/Formats/DictionaryConverter.js";
import { defaultName, dictionarySources, stem } from "../Domain/Formats/DictionaryFormat.js";
import { normalizeWord } from "../Domain/Dictionary/WordNormalizer.js";
import { SqliteFile } from "../Support/SqliteFile.js";
import { now } from "../Support/Text.js";
import { packMetadata } from "./DictionaryDatabase.js";

const byAge = (a, b) => (a.addedAt < b.addedAt ? -1 : a.addedAt > b.addedAt ? 1 : a.id - b.id);

export class DictionaryPacks {
  #db;
  #dictionary;

  constructor(db, dictionary) {
    this.#db = db;
    this.#dictionary = dictionary;
  }

  async all() {
    return (await this.#db.getAll("dictionaryPacks")).sort(byAge);
  }

  async enabled() {
    return (await this.all()).filter((pack) => pack.isEnabled);
  }

  async setEnabled(id, isEnabled) {
    const pack = await this.#db.get("dictionaryPacks", id);
    if (pack) await this.#db.put("dictionaryPacks", { ...pack, isEnabled });
  }

  /** Removes the pack and what it holds. The bundled pack cannot be removed. */
  async remove(pack) {
    if (pack.kind === "bundled") return;
    await this.#db.delete("dictionaryPacks", pack.id);
    await this.#db.delete("dictionaryFiles", pack.id);
    if (pack.kind === "converted") await this.#db.deleteRange("dictionaryArticles", [pack.id, ""], [pack.id, []]);
    this.#dictionary.close(pack.id);
  }

  /**
   * Adds the dictionaries among the picked files — StarDict's three files
   * picked together. Returns `{ added, errors }`; one already in the list
   * is left alone.
   */
  async addFiles(files) {
    const sources = await dictionarySources(files);
    const used = new Set(sources.flatMap((source) => [source.main, ...source.companions]));
    const errors = files.filter((file) => !used.has(file)).map((file) => `${file.name} is not a dictionary this app can read.`);
    const known = new Set((await this.all()).map((pack) => pack.fileName));
    let added = 0;
    for (const source of sources) {
      if (known.has(source.main.name)) continue;
      try {
        if (source.format === "native") await this.#addNative(source.main);
        else await this.#convert(source);
        known.add(source.main.name);
        added++;
      } catch (error) {
        errors.push(error.message);
      }
    }
    return { added, errors };
  }

  async #insert(pack) {
    return this.#db.put("dictionaryPacks", { ...pack, isEnabled: true, addedAt: now() });
  }

  /** A pack this app wrote is taken as it is, once its schema checks out. */
  async #addNative(file) {
    const unreadable = new Error(`${file.name} is not a dictionary this app can read.`);
    if (!(await SqliteFile.isDatabase(file))) throw unreadable;
    let metadata;
    try {
      metadata = await packMetadata(file);
    } catch {
      throw unreadable;
    }
    const version = metadata.schema_version ?? "?";
    if (version !== "2") throw new Error(`${file.name} uses schema version ${version}; this app reads version 2.`);
    const id = await this.#insert({
      name: stem(file.name).replace(/\.converted$/, ""),
      kind: "file",
      fileName: file.name,
      targetLanguage: metadata.target_language ?? "",
      definitionLanguage: metadata.definition_language ?? "",
    });
    await this.#db.put("dictionaryFiles", file, id);
  }

  /** A dictionary in another format becomes articles filed by headword. */
  async #convert(source) {
    const articles = new Map();
    const info = await convertDictionary(source, (entry) => {
      const word = normalizeWord(entry.headword);
      const senses = entry.senses.map((sense) => sense.trim()).filter(Boolean);
      if (!word || !senses.length) return;
      if (!articles.has(word)) articles.set(word, []);
      articles.get(word).push({ partOfSpeech: entry.partOfSpeech, senses });
    });
    if (!articles.size) throw new Error(`${source.main.name} held no entries this app could read.`);
    const id = await this.#insert({
      name: info.name || defaultName(source),
      kind: "converted",
      fileName: source.main.name,
      targetLanguage: info.targetLanguage,
      definitionLanguage: info.definitionLanguage,
    });
    const rows = [...articles].map(([word, list]) => ({ packId: id, word, articles: list }));
    for (let start = 0; start < rows.length; start += 5000) await this.#db.putAll("dictionaryArticles", rows.slice(start, start + 5000));
  }
}

/** "fr → ru" when the pack says what it holds. */
export function packLanguages(pack) {
  return pack.targetLanguage && pack.definitionLanguage ? `${pack.targetLanguage} → ${pack.definitionLanguage}` : "";
}
