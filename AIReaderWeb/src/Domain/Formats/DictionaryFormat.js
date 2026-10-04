// The dictionary file formats the app reads, and how a set of picked files
// sorts into dictionaries. Only StarDict spreads itself over several files.
// A source is `{ format, main, companions }`, the files being `File`s.

export const formatLabels = {
  native: "AIReader pack",
  delimited: "tab- or comma-separated",
  xdxf: "XDXF",
  dsl: "Lingvo DSL",
  stardict: "StarDict",
};

/** The file name without its extension, seeing through `.dz` and `.gz`. */
export function stem(name) {
  const unwrapped = name.replace(/\.(dz|gz)$/i, "");
  const dot = unwrapped.lastIndexOf(".");
  return dot <= 0 ? unwrapped : unwrapped.slice(0, dot);
}

function extension(name) {
  const unwrapped = name.toLowerCase().replace(/\.(dz|gz)$/, "");
  const dot = unwrapped.lastIndexOf(".");
  return dot < 0 ? "" : unwrapped.slice(dot + 1);
}

/** Reads the first bytes, for formats whose extension is unhelpful. */
async function sniff(file) {
  const head = new TextDecoder("latin1").decode(await file.slice(0, 512).arrayBuffer());
  if (head.startsWith("SQLite format 3")) return "native";
  const lowered = head.toLowerCase();
  if (lowered.includes("<xdxf")) return "xdxf";
  if (lowered.startsWith("#name")) return "dsl";
  return null;
}

/** The format of a single file, or null when it is a companion or unknown. */
export async function formatOf(file) {
  const ext = extension(file.name);
  if (["sqlite3", "sqlite", "db"].includes(ext)) return "native";
  if (["tsv", "csv", "txt"].includes(ext)) return "delimited";
  if (ext === "xdxf") return "xdxf";
  if (ext === "dsl") return "dsl";
  if (ext === "ifo") return "stardict";
  if (ext === "xml") return (await sniff(file)) === "xdxf" ? "xdxf" : null;
  if (["idx", "dict", "syn"].includes(ext)) return null;
  return sniff(file);
}

/** Sorts files into the dictionaries they make up; StarDict's `.idx` and `.dict` join the `.ifo` they came with. */
export async function dictionarySources(files) {
  const sources = [];
  for (const file of files) {
    const format = await formatOf(file);
    if (format) sources.push({ format, main: file, companions: [] });
  }
  for (const source of sources) {
    if (source.format !== "stardict") continue;
    const base = stem(source.main.name);
    source.companions = files.filter((file) => file !== source.main && stem(file.name) === base);
  }
  return sources;
}

/** The companion whose name ends in `suffix`, ignoring a trailing `.dz`. */
export function companion(source, suffix) {
  return source.companions.find((file) => file.name.toLowerCase().replace(/\.dz$/, "").endsWith(suffix)) ?? null;
}

/** The name to fall back on when the file itself carries none. */
export function defaultName(source) {
  return stem(source.main.name).replace(/\.dsl$/i, "");
}

const languageNames = {
  fre: "fr", fra: "fr", french: "fr", eng: "en", english: "en", ger: "de", deu: "de", german: "de",
  spa: "es", spanish: "es", ita: "it", italian: "it", por: "pt", portuguese: "pt", rus: "ru", russian: "ru",
  dut: "nl", nld: "nl", dutch: "nl", pol: "pl", polish: "pl", ukr: "uk", ukrainian: "uk", jpn: "ja",
  japanese: "ja", chi: "zh", zho: "zh", chinese: "zh", tur: "tr", turkish: "tr", swe: "sv", swedish: "sv",
  lat: "la", latin: "la", gre: "el", greek: "el",
};

/** Whatever a dictionary calls its language, as a two-letter code, or "". */
export function languageNameCode(raw) {
  const text = (raw ?? "").trim().toLowerCase();
  if (text.length === 2) return text;
  return languageNames[text] ?? "";
}
