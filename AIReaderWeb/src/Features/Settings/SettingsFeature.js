// Where lookups are sent: the endpoint, the token, and which of the models
// it offers to use. Where the model may search the web. Where the library
// is shared with the other devices. Changes are saved as they are made.
import { models } from "../../Services/ChatApi.js";
import { syncConfigured } from "../../Services/Settings.js";
import { runSync } from "../../Services/Sync.js";
import { Feature } from "../Common/Feature.js";

export class SettingsFeature extends Feature {
  models = [];
  isLoadingModels = false;
  isSyncing = false;
  error = "";
  /** What the last sync did, or why it could not. */
  syncMessage = "";
  #env;

  constructor(env) {
    super();
    this.#env = env;
    this.ai = env.settings.ai();
    this.web = env.settings.web();
    this.sync = env.settings.sync();
  }

  get canSync() {
    return syncConfigured(this.sync) && !this.isSyncing;
  }

  setAi(field, value) {
    if (this.ai[field] === value) return;
    this.ai = { ...this.ai, [field]: value };
    // A model list belongs to the endpoint it came from.
    if (field === "endpoint") Object.assign(this, { models: [], error: "" });
    this.#env.settings.saveAi(this.ai);
    this.changed();
  }

  setWeb(field, value) {
    this.web = { ...this.web, [field]: value };
    this.#env.settings.saveWeb(this.web);
  }

  setSync(field, value) {
    this.sync = { ...this.sync, [field]: value };
    this.#env.settings.saveSync(this.sync);
    this.changed();
  }

  async loadModels() {
    if (this.isLoadingModels) return;
    Object.assign(this, { isLoadingModels: true, error: "" });
    this.changed();
    try {
      this.models = await models(this.ai);
      if (this.models.length && !this.models.includes(this.ai.model)) this.setAi("model", this.models[0]);
    } catch (error) {
      this.error = error.message;
    }
    this.isLoadingModels = false;
    this.changed();
  }

  /** Fetches the server's document, merges, applies, and sends it back. */
  async syncNow() {
    if (!this.canSync) return;
    Object.assign(this, { isSyncing: true, syncMessage: "" });
    this.changed();
    const result = await runSync(this.#env);
    Object.assign(this, { isSyncing: false, syncMessage: result?.message ?? "" });
    this.changed();
  }
}
