// The reader's settings: the model, the web search, the sync folder, the
// reading style, the language of the last page photographed, which the
// next photo is read in first, and whether a PDF's pages mark their words. Kept in the browser's local storage — tokens
// and the sync password too, as the Kindle app keeps them in its settings
// file; the browser does not encrypt it. Values given in the page's address
// for one run (`?aiEndpoint=mock://ai`) win over the saved ones without
// replacing them.
import { defaultStyle, sanitizeStyle } from "../Domain/Reading/ReadingStyle.js";
import { booksFolder } from "../Domain/Books/RemoteBookName.js";
import { oldSyncFile, partsFolder } from "../Domain/Sync/SyncParts.js";

/** Not a real address: lookups are answered by `MockAI`, without the network. */
export const mockEndpoint = "mock://ai";

export const defaultAi = { endpoint: "https://api.mistral.ai/v1", apiKey: "", openAIKey: "", model: "mistral-medium-3.5", language: "Russian" };
/** TinyFish's search, which Monid lists at no charge. */
export const defaultWeb = { apiKey: "", provider: "tinyfish", endpoint: "/search", input: '{"queryParams": {"query": "$query", "language": "$language"}}' };
export const defaultSync = { url: "", username: "", password: "" };

const isOpenAI = (ai) => ai.endpoint.toLowerCase().includes("api.openai.com");
export const usesMock = (ai) => ai.endpoint.trim() === mockEndpoint;
/** The token for the current endpoint: OpenAI's own is kept apart, so switching services means no retyping. */
export const aiToken = (ai) => (isOpenAI(ai) && ai.openAIKey ? ai.openAIKey : ai.apiKey);

/** An endpoint whose address names Anthropic — its own, or a gateway's to it — speaks its Messages API (`AnthropicMessages.js`). */
export const usesAnthropic = (ai) => ai.endpoint.toLowerCase().includes("anthropic");

const base = (url) => url.trim().replace(/\/+$/, "");
export const chatUrl = (ai) => `${base(ai.endpoint)}/${usesAnthropic(ai) ? "messages" : "chat/completions"}`;
// Anthropic lists twenty models at a time unless asked for more.
export const modelsUrl = (ai) => `${base(ai.endpoint)}/models${usesAnthropic(ai) ? "?limit=1000" : ""}`;

export const syncConfigured = (sync) => sync.url.trim() !== "";
/** The folder of record files (`SyncParts.js`), and the one file older versions kept the records in. */
export const syncPartsUrl = (sync) => `${base(sync.url)}/${partsFolder}/`;
export const syncOldFileUrl = (sync) => `${base(sync.url)}/${oldSyncFile}`;
export const syncBooksUrl = (sync) => `${base(sync.url)}/${booksFolder}/`;

export const webConfigured = (web) => !!(web.apiKey.trim() && web.provider.trim() && web.endpoint.trim());

/** The web search input with the query and language in place, escaped so they cannot break out of the JSON. */
export function webRequest(web, query, language) {
  const escaped = (value) => JSON.stringify(value).slice(1, -1);
  return web.input.replaceAll("$query", escaped(query)).replaceAll("$language", escaped(language || "en"));
}

export class SettingsStore {
  #storage;
  #run;

  /** `storage` is `localStorage` or alike; `run` the values given for this run only. */
  constructor(storage, run = {}) {
    this.#storage = storage;
    this.#run = run;
  }

  /** Reads `aiEndpoint`, `aiModel`, `syncURL` and `syncUsername` out of a page address's query. */
  static runValues(search) {
    const parameters = new URLSearchParams(search);
    const value = (name) => parameters.get(name) || undefined;
    return { endpoint: value("aiEndpoint"), model: value("aiModel"), syncURL: value("syncURL"), syncUsername: value("syncUsername") };
  }

  #read(key, defaults) {
    let saved = {};
    try {
      saved = JSON.parse(this.#storage.getItem(`aireader.${key}`) ?? "{}") ?? {};
    } catch {}
    const result = { ...defaults };
    for (const field of Object.keys(defaults)) if (typeof saved[field] === typeof defaults[field]) result[field] = saved[field];
    return result;
  }

  #write(key, value) {
    this.#storage.setItem(`aireader.${key}`, JSON.stringify(value));
  }

  ai() {
    const settings = this.#read("ai", defaultAi);
    // A cleared field would leave the model no language to answer in.
    if (!settings.language.trim()) settings.language = defaultAi.language;
    if (this.#run.endpoint) settings.endpoint = this.#run.endpoint;
    if (this.#run.model) settings.model = this.#run.model;
    return settings;
  }

  saveAi(settings) {
    // A value given for this run is not the reader's setting; the saved one stays.
    const saved = this.#read("ai", defaultAi);
    this.#write("ai", {
      ...settings,
      endpoint: this.#run.endpoint ? saved.endpoint : settings.endpoint,
      model: this.#run.model ? saved.model : settings.model,
    });
  }

  web() {
    return this.#read("web", defaultWeb);
  }

  saveWeb(settings) {
    this.#write("web", settings);
  }

  sync() {
    const settings = this.#read("sync", defaultSync);
    if (this.#run.syncURL) settings.url = this.#run.syncURL;
    if (this.#run.syncUsername) settings.username = this.#run.syncUsername;
    return settings;
  }

  saveSync(settings) {
    const saved = this.#read("sync", defaultSync);
    this.#write("sync", {
      ...settings,
      url: this.#run.syncURL ? saved.url : settings.url,
      username: this.#run.syncUsername ? saved.username : settings.username,
    });
  }

  style() {
    return sanitizeStyle(this.#read("style", defaultStyle));
  }

  saveStyle(style) {
    this.#write("style", style);
  }

  /** The language the last photographed page was in, a two-letter code; "" before the first photo. */
  photoLanguage() {
    return this.#read("photo", { language: "" }).language;
  }

  savePhotoLanguage(language) {
    this.#write("photo", { language });
  }

  /** Whether a PDF's page view marks every word a tap finds; off until the reader turns it on. */
  pageWords() {
    return this.#read("pages", { showsWords: false }).showsWords;
  }

  savePageWords(showsWords) {
    this.#write("pages", { showsWords });
  }
}
