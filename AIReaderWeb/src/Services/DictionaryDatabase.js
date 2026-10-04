// Searches the dictionary packs, the way the other apps' `DictionaryDatabase`
// queries them: the form table first, for the lemmas a written form belongs
// to, then the headword itself, then the articles under each lemma. An
// SQLite pack is read where it lies; a converted one from its articles.
import { emptyLookup } from "../Domain/Dictionary/DictionaryLookup.js";
import { normalizeWord } from "../Domain/Dictionary/WordNormalizer.js";
import { zlib } from "../Support/Inflate.js";
import { SqliteFile } from "../Support/SqliteFile.js";

const decoder = new TextDecoder();

/** One article as a pack stores it: JSON, usually zlib-compressed, with `part_of_speech` and `definitions[].glosses[]`. */
async function article(payload, lemma, source) {
  const bytes = typeof payload === "string" ? new TextEncoder().encode(payload) : payload;
  let json;
  try {
    json = JSON.parse(decoder.decode((await zlib(bytes)) ?? bytes));
  } catch {
    return null;
  }
  const senses = (json?.definitions ?? []).flatMap((definition) => definition?.glosses ?? []).filter((gloss) => typeof gloss === "string");
  if (!senses.length) return null;
  return { lemma, partOfSpeech: typeof json.part_of_speech === "string" ? json.part_of_speech : "", senses, source };
}

function features(verbInfo) {
  try {
    const list = JSON.parse(verbInfo);
    return Array.isArray(list) ? list.filter((item) => typeof item === "string") : [];
  } catch {
    return [];
  }
}

/** A pack file opened, with where its tables start. */
async function openPack(blob) {
  const file = await SqliteFile.open(blob);
  const schema = await file.schema();
  const root = (name) => schema.find((row) => row.type === "table" && row.name === name)?.rootPage ?? 0;
  const lemmaIndex = schema.find((row) => row.type === "index" && row.tableName === "lemmas")?.rootPage ?? 0;
  return { file, forms: root("forms"), lemmas: root("lemmas"), lemmaIndex, entries: root("entries"), metadata: root("metadata") };
}

/** The `metadata` table of a pack file, as an object; throws when it is not a pack. */
export async function packMetadata(blob) {
  const pack = await openPack(blob);
  if (!pack.metadata) throw new Error("The file has no metadata table.");
  return Object.fromEntries((await pack.file.tableRows(pack.metadata)).map(([, [key, value]]) => [key, String(value ?? "")]));
}

export class DictionaryDatabase {
  #db;
  #bundled;
  #opened = new Map();

  /** `bundled` gives the Blob of the pack that ships with the app. */
  constructor(db, bundled) {
    this.#db = db;
    this.#bundled = bundled;
  }

  /** Forgets an opened pack, as when it is removed. */
  close(packId) {
    this.#opened.delete(packId);
  }

  #open(pack) {
    if (!this.#opened.has(pack.id)) {
      const opening = (async () => {
        const blob = pack.kind === "bundled" ? await this.#bundled() : await this.#db.get("dictionaryFiles", pack.id);
        return blob ? openPack(blob) : null;
      })();
      // A failed open is tried again next time, the file may have arrived.
      opening.catch(() => this.#opened.delete(pack.id));
      this.#opened.set(pack.id, opening);
    }
    return this.#opened.get(pack.id);
  }

  /** Searches every pack given and merges what they say about the word. */
  async lookup(word, packs) {
    const normalized = normalizeWord(word);
    const result = emptyLookup(normalized);
    if (!normalized) return result;
    for (const pack of packs) {
      if (pack.kind === "converted") await this.#searchConverted(pack, normalized, result);
      else await this.#searchFile(pack, normalized, result);
    }
    return result;
  }

  /** The articles filed under exactly this headword: the entry a lemma came from. A form of another word brings nothing. */
  async articlesFor(lemma, packs) {
    const wanted = normalizeWord(lemma);
    return (await this.lookup(lemma, packs)).articles.filter((found) => normalizeWord(found.lemma) === wanted);
  }

  async #searchConverted(pack, normalized, result) {
    const row = await this.#db.get("dictionaryArticles", [pack.id, normalized]);
    for (const found of row?.articles ?? []) {
      result.articles.push({ lemma: normalized, partOfSpeech: found.partOfSpeech, senses: found.senses, source: pack.name });
    }
  }

  async #searchFile(pack, normalized, result) {
    let opened;
    try {
      opened = await this.#open(pack);
    } catch {
      return;
    }
    if (!opened || !opened.forms || !opened.lemmas || !opened.entries) return;
    const { file } = opened;
    const forms = await file.indexRows(opened.forms, [normalized]);
    const named = async () => {
      if (opened.lemmaIndex) return (await file.indexRows(opened.lemmaIndex, [normalized])).map(([lemma, id]) => ({ id, word: lemma }));
      return (await file.tableRows(opened.lemmas)).filter(([, values]) => values[1] === normalized).map(([id, values]) => ({ id, word: values[1] }));
    };
    let lemmas = [];
    if (forms.length) {
      const ids = [...new Set(forms.map((row) => row[3]))].sort((a, b) => a - b);
      for (const id of ids) {
        const row = await file.row(opened.lemmas, id);
        if (row) lemmas.push({ id, word: row[1] });
      }
    }
    // A form whose lemma has no article is still worth reporting; the
    // headword itself may carry the definitions.
    if (!lemmas.length) lemmas = await named();
    const names = new Map(lemmas.map((lemma) => [lemma.id, lemma.word]));
    for (const [, , , lemmaId, partOfSpeech, gender, number, verbInfo] of forms) {
      if (!names.has(lemmaId)) continue;
      result.forms.push({ lemma: names.get(lemmaId), partOfSpeech: partOfSpeech ?? "", gender: gender ?? "", number: number ?? "", features: features(verbInfo ?? "") });
    }
    for (const lemma of lemmas) {
      for (const [, , payload] of await file.indexRows(opened.entries, [lemma.id])) {
        const found = await article(payload, lemma.word, pack.name);
        if (found) result.articles.push(found);
      }
    }
  }
}
